#!/usr/bin/env python3
"""Send multicast_ota.py-style OTA messages inside length-prefixed PROPAG OTA frames.

Positional arguments intentionally match multicast_ota.py, but <ipv6> is only
kept for compatibility. Real PROPAG targets are read from `wsbrd_cli status`,
like send_br_emergency.py:
  propagation_ota.py <ignored_ipv6> <port> <gbl_filename|command> <tag> <interval_s> <chunks_selection...>

Extra propagation options can be appended, for example:
  --NON
  --CON --timeout 10000
  --broadcast_linklocal --interface tun0
"""

from __future__ import annotations

import argparse
import ipaddress
import os
import shlex
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from time import localtime, strftime


DISCRIMINATOR = "OTA"
LINKLOCAL_ALL_NODES = "ff02::1"
TFTP_FOLDER = "/srv/tftp/"
DEFAULT_STATUS_COMMAND = "wsbrd_cli status"
DEFAULT_CHILD_SEND_DELAY_S = 0.2

PROPAGATION_FRAME_MAX_BYTES = 1232
PROPAGATION_PAYLOAD_MAX_BYTES = 1200
DEFAULT_CHUNK_SIZE = 1024


TEST_OPTIONS_1ARG = {
    "show_missed_from_list()",
    "show_missed()",
    "show_repeated()",
    "clear_ota_data()",
    "verify_image_in_flash()",
    "setImageToBootload()",
}

TEST_OPTIONS_2ARG = {
    "rebootAndInstall()",
    "rebootAndInstallClearNVMApp()",
    "rebootAndInstallClearNVMFull()",
}


@dataclass
class ChunkSelection:
    last_chunk: int
    only_mode: bool = False
    only_chunk: int = 0
    min_mode: bool = False
    min_chunk: int = 0
    and_mode: bool = False
    and_chunks: set[int] | None = None


def now(format_str: str = "%H:%M:%S") -> str:
    return strftime(format_str, localtime())


def log(message: str) -> None:
    print(f"[{now('%Y-%m-%d_%H:%M:%S')}] {message}")


def ipv6_or_none(text: str) -> str | None:
    try:
        return str(ipaddress.IPv6Address(text))
    except ValueError:
        return None


def ipv6_last4(addr: str) -> str:
    return ipaddress.IPv6Address(addr).packed[-2:].hex()


def interface_scope_id(interface: str | None) -> int:
    if not interface:
        return 0
    try:
        return int(interface, 10)
    except ValueError:
        return socket.if_nametoindex(interface)


def run_status_command(command: str) -> str:
    result = subprocess.run(
        shlex.split(command),
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return result.stdout


def read_status(args: argparse.Namespace) -> str:
    if args.status_file:
        return Path(args.status_file).read_text(encoding="utf-8")
    return run_status_command(args.status_command)


def parse_topology(status_text: str) -> tuple[str, list[str], dict[str, int], int]:
    br_addr: str | None = None
    children: list[str] = []
    branch_sizes: dict[str, int] = {}
    current_direct_child: str | None = None
    pan_size = 0

    for line in status_text.splitlines():
        stripped = line.strip()
        if not stripped:
            continue

        if br_addr is None:
            candidate = ipv6_or_none(stripped)
            if candidate is not None:
                br_addr = candidate
            continue

        tokens = stripped.split()
        if not tokens:
            continue

        candidate = ipv6_or_none(tokens[-1])
        if candidate is None:
            continue

        pan_size += 1
        marker_pos = max(line.rfind("|-"), line.rfind("`-"))
        if marker_pos == 2:
            children.append(candidate)
            branch_sizes[candidate] = 0
            current_direct_child = candidate
        elif marker_pos > 2 and current_direct_child is not None:
            branch_sizes[current_direct_child] += 1

    if br_addr is None:
        raise RuntimeError("Could not find the Border Router IPv6 address in wsbrd_cli status output")

    return br_addr, children, branch_sizes, pan_size


def build_propag_frame(confirmable: bool,
                       payload: bytes,
                       timeout_ms: int,
                       sequence: int,
                       algo_selected: int,
                       pan_size: int) -> bytes:
    if len(payload) > PROPAGATION_PAYLOAD_MAX_BYTES:
        raise ValueError(
            f"PROPAG payload is {len(payload)} bytes, "
            f"max is {PROPAGATION_PAYLOAD_MAX_BYTES}"
        )

    if confirmable:
        frame = (
            f"PROPAG IND_CON {sequence} {algo_selected} {pan_size} "
            f"{DISCRIMINATOR} {len(payload)} {timeout_ms} "
        ).encode("ascii") + payload
    else:
        frame = (
            f"PROPAG IND_NON {sequence} {algo_selected} {pan_size} "
            f"{DISCRIMINATOR} {len(payload)} "
        ).encode("ascii") + payload

    if len(frame) > PROPAGATION_FRAME_MAX_BYTES:
        raise ValueError(
            f"PROPAG frame is {len(frame)} bytes, max is {PROPAGATION_FRAME_MAX_BYTES}; "
            "reduce --chunk-size"
        )

    return frame


def make_socket(bind_port: int | None) -> socket.socket:
    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if bind_port is not None:
        sock.bind(("::", bind_port))
    return sock


def send_frame(sock: socket.socket, dest: str, port: int, frame: bytes, scope_id: int) -> None:
    sock.sendto(frame, (dest, port, 0, scope_id))
    print(f"Sent {len(frame)} bytes to {dest}/{port}")


def parse_confirmation(data: bytes, expected_sequence: int) -> tuple[int, int, int, str, int, str] | None:
    text = data.decode("ascii", errors="replace").strip()
    parts = text.split(maxsplit=8)
    if len(parts) < 8:
        return None
    if parts[0] != "PROPAG" or parts[1] != "CONF" or parts[5] != DISCRIMINATOR:
        return None
    try:
        sequence = int(parts[2], 10)
        algo_selected = int(parts[3], 10)
        pan_size = int(parts[4], 10)
        count = int(parts[7], 10)
    except ValueError:
        return None
    if sequence != expected_sequence:
        return None
    option = parts[8] if len(parts) >= 9 else ""
    return sequence, algo_selected, pan_size, parts[6], count, option


def expected_child_from_sender(sender: str, children: list[str]) -> str | None:
    if sender in children:
        return sender

    sender_tag = ipv6_last4(sender)
    matches = [child for child in children if ipv6_last4(child) == sender_tag]
    if len(matches) == 1:
        return matches[0]
    return None


def missing_tokens_from_option(option: str) -> set[str]:
    missing: set[str] = set()
    for chunk in option.replace(";", " ").split():
        if chunk.startswith("MISS="):
            chunk = chunk[len("MISS="):]
        for token in chunk.split(","):
            token = token.strip()
            if token:
                missing.add(token)
    return missing


def receive_confirmations(sock: socket.socket,
                          children: list[str],
                          timeout_ms: int,
                          start_time: float,
                          label: str,
                          sequence: int) -> int:
    deadline = time.monotonic() + (timeout_ms / 1000.0)
    confirmations: dict[str, tuple[int, int, int, str, int, str]] = {}
    reported_missing: set[str] = set()
    last_confirmation_time: float | None = None

    while set(children) - confirmations.keys():
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        sock.settimeout(remaining)
        try:
            data, addr = sock.recvfrom(2048)
        except socket.timeout:
            break

        sender = str(ipaddress.IPv6Address(addr[0]))
        text = data.decode("ascii", errors="replace").strip()
        parsed = parse_confirmation(data, sequence)
        if parsed is None:
            log(f"ignored UDP from {sender}: {text}")
            continue

        rx_sequence, algo_selected, pan_size, payload, count, option = parsed
        expected_child = expected_child_from_sender(sender, children)
        if expected_child is None:
            log(f"ignored CONF from non-direct child {sender}: {text}")
            continue
        if expected_child in confirmations:
            log(f"ignored duplicate CONF from {sender} ({ipv6_last4(sender)})")
            continue

        confirmations[expected_child] = parsed
        reported_missing.update(missing_tokens_from_option(option))
        last_confirmation_time = time.monotonic()
        log(
            f"CONF from {sender} ({ipv6_last4(sender)}): "
            f"seq={rx_sequence} algo={algo_selected} pan={pan_size} "
            f"payload={payload} count={count} option={option or '-'}"
        )

    no_response = set(children) - confirmations.keys()
    local_missing = {ipv6_last4(addr) for addr in no_response}
    all_missing = sorted(local_missing | reported_missing)
    total_count = sum(count for _sequence, _algo, _pan, _payload, count, _option in confirmations.values())
    elapsed_end = last_confirmation_time if len(confirmations) == len(children) else time.monotonic()
    elapsed_ms = (elapsed_end - start_time) * 1000.0
    label_text = f" {label}" if label else ""

    print(f"direct child confirmations{label_text}: {len(confirmations)}/{len(children)}")
    print(f"direct child confirmation elapsed{label_text}: {elapsed_ms:.1f} ms")
    print(f"aggregated confirmed nodes below BR{label_text}: {total_count}")
    print(f"missing tags{label_text}: {','.join(all_missing) if all_missing else 'none'}")

    return 0 if not all_missing and len(confirmations) == len(children) else 2


def send_ota_message(sock: socket.socket,
                     children: list[str],
                     port: int,
                     ota_message: bytes,
                     args: argparse.Namespace,
                     scope_id: int,
                     sequence: int,
                     algo_selected: int,
                     pan_size: int,
                     chunk_index: int | None = None) -> int:
    frame = build_propag_frame(args.confirmable,
                               ota_message,
                               args.timeout,
                               sequence,
                               algo_selected,
                               pan_size)
    label = f"chunk {chunk_index}" if chunk_index is not None else "command"

    if args.dry_run:
        target = LINKLOCAL_ALL_NODES if args.broadcast_linklocal else f"{len(children)} direct children"
        print(
            f"DRY RUN {label}: target={target} seq={sequence} "
            f"frame_len={len(frame)} payload_len={len(ota_message)}"
        )
        return 0

    start_time = time.monotonic()
    if args.broadcast_linklocal:
        send_frame(sock, LINKLOCAL_ALL_NODES, port, frame, scope_id)
    else:
        for child in children:
            send_frame(sock, child, port, frame, 0)
            time.sleep(args.child_send_delay)

    if args.confirmable:
        return receive_confirmations(sock, children, args.timeout, start_time, label, sequence)

    return 0


def parse_chunk_selection(chunk_args: list[str]) -> ChunkSelection:
    if not chunk_args:
        return ChunkSelection(last_chunk=0)

    first = int(chunk_args[0])
    if len(chunk_args) >= 2 and chunk_args[1] == "only":
        return ChunkSelection(last_chunk=first, only_mode=True, only_chunk=first)

    if len(chunk_args) >= 2 and chunk_args[1] == "min":
        return ChunkSelection(last_chunk=0, min_mode=True, min_chunk=first)

    if len(chunk_args) >= 2 and chunk_args[1] == "and":
        chunks = {first}
        chunks.update(int(item) for item in chunk_args[2:])
        return ChunkSelection(last_chunk=max(chunks), and_mode=True, and_chunks=chunks)

    return ChunkSelection(last_chunk=first)


def should_send_chunk(chunk_index: int, selection: ChunkSelection) -> bool:
    if selection.only_mode:
        return chunk_index == selection.only_chunk
    if selection.and_mode:
        return selection.and_chunks is not None and chunk_index in selection.and_chunks
    return chunk_index >= selection.min_chunk


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send multicast_ota.py-compatible OTA payloads wrapped in PROPAG OTA frames."
    )
    parser.add_argument("ipv6", help="ignored compatibility IPv6 argument from multicast_ota.py")
    parser.add_argument("port", type=int, help="destination UDP propagation port")
    parser.add_argument("gbl_filename", help="GBL filename or OTA command")
    parser.add_argument("tag", help="OTA board tag")
    parser.add_argument("interval_s", type=float, help="delay between chunks")
    parser.add_argument("chunk_args", nargs="*", help="same chunk selection arguments as multicast_ota.py")

    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--CON", dest="confirmable", action="store_true", help="send PROPAG IND_CON")
    mode.add_argument("--NON", dest="confirmable", action="store_false", help="send PROPAG IND_NON")
    parser.set_defaults(confirmable=False)

    parser.add_argument("--timeout", type=int, default=10000, help="IND_CON timeout and CONF wait in ms")
    parser.add_argument("--sequence", type=int, help="PROPAG base sequence; default is current time modulo 65536")
    parser.add_argument(
        "--algo",
        "--algo-selected",
        dest="algo_selected",
        type=int,
        choices=(0, 1),
        default=None,
        help="PROPAG algorithm selector; default is 1 with --broadcast_linklocal, otherwise 0",
    )
    parser.add_argument("--chunk-size", type=int, default=DEFAULT_CHUNK_SIZE, help="GBL chunk bytes before PROPAG wrapping")
    parser.add_argument("--status-command", default=DEFAULT_STATUS_COMMAND, help="command used to read BR topology")
    parser.add_argument("--status-file", help="parse topology from a saved status file instead of running wsbrd_cli")
    parser.add_argument(
        "--big_branch_first",
        action="store_true",
        help="send first to direct children with the largest direct/indirect subtree",
    )
    parser.add_argument(
        "--broadcast_linklocal",
        action="store_true",
        help="send one UDP frame to ff02::1 instead of unicast to each direct child",
    )
    parser.add_argument("--interface", help="outgoing interface name or index for link-local multicast")
    parser.add_argument("--child-send-delay", type=float, default=DEFAULT_CHILD_SEND_DELAY_S,
                        help="delay in seconds between direct-child unicast sends")
    parser.add_argument("--dry-run", action="store_true", help="build and print frames without sending UDP")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.timeout < 0:
        raise ValueError("--timeout must be >= 0")
    if args.chunk_size <= 0:
        raise ValueError("--chunk-size must be > 0")
    if args.child_send_delay < 0:
        raise ValueError("--child-send-delay must be >= 0")
    if args.sequence is not None and not 0 <= args.sequence <= 0xFFFF:
        raise ValueError("--sequence must be between 0 and 65535")

    status_text = read_status(args)
    br_addr, children, branch_sizes, pan_size = parse_topology(status_text)
    if args.big_branch_first and not args.broadcast_linklocal:
        children = sorted(children, key=lambda addr: branch_sizes.get(addr, 0), reverse=True)

    scope_id = interface_scope_id(args.interface)
    bind_port = args.port if args.confirmable else None
    selection = parse_chunk_selection(args.chunk_args)
    test_option = args.gbl_filename
    base_sequence = args.sequence if args.sequence is not None else int(time.time() * 1000.0) & 0xFFFF
    algo_selected = args.algo_selected if args.algo_selected is not None else (1 if args.broadcast_linklocal else 0)

    print(f"BR: {br_addr}")
    print(f"Ignored positional IPv6: {args.ipv6}")
    print(f"PAN size: {pan_size}")
    print(f"PROPAG base sequence: {base_sequence}")
    print(f"PROPAG algo_selected: {algo_selected}")
    print(
        "direct children:",
        ", ".join(
            f"{addr} ({ipv6_last4(addr)}, branch={branch_sizes.get(addr, 0)})"
            for addr in children
        ) or "none",
    )
    print(f"Port: {args.port}")
    print(f"GBL Filename: {args.gbl_filename}")
    print(f"Tag: {args.tag}")
    print(f"Interval (s): {args.interval_s}")
    print(f"PROPAG mode: {'IND_CON' if args.confirmable else 'IND_NON'}")
    if args.broadcast_linklocal:
        print(f"send mode: link-local broadcast {LINKLOCAL_ALL_NODES}")
        if scope_id:
            print(f"broadcast interface scope_id: {scope_id}")
    elif args.big_branch_first:
        print("send order: biggest branch first")

    if not children:
        print("No direct children found; nothing to send.")
        return 0

    with make_socket(bind_port) as sock:
        if args.broadcast_linklocal:
            if scope_id != 0:
                sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_IF, scope_id)
            sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_HOPS, 1)

        if test_option in TEST_OPTIONS_1ARG:
            message = f"OTA {test_option} " + 50 * "+"
            print(f"Message mode: {message}")
            return send_ota_message(sock,
                                    children,
                                    args.port,
                                    message.encode("utf-8"),
                                    args,
                                    scope_id,
                                    base_sequence,
                                    algo_selected,
                                    pan_size)

        if test_option in TEST_OPTIONS_2ARG:
            message = f"OTA {test_option} {selection.last_chunk} " + 50 * "+"
            print(f"Message mode: {message}")
            return send_ota_message(sock,
                                    children,
                                    args.port,
                                    message.encode("utf-8"),
                                    args,
                                    scope_id,
                                    base_sequence,
                                    algo_selected,
                                    pan_size)

        gbl_path = os.path.join(TFTP_FOLDER, args.gbl_filename)
        if not os.path.isfile(gbl_path):
            raise FileNotFoundError(f"No such file as '{args.gbl_filename}' under {TFTP_FOLDER}")

        with open(gbl_path, "rb") as gbl_file:
            filebytes = gbl_file.read()

        file_data_len = len(filebytes)
        max_chunk = int(file_data_len / args.chunk_size + 1)
        print(f"file size: {file_data_len} bytes, to be sent in {max_chunk} chunks of {args.chunk_size} bytes")

        chunk_offset = 0
        chunk_index = 1
        last_sent_data_len = 0

        while chunk_offset < file_data_len:
            chunk_data = filebytes[chunk_offset:chunk_offset + args.chunk_size]
            chunk_data_len = len(chunk_data)
            pre_header = f"OTA {args.gbl_filename} {chunk_index:4d} {0:4d} {now()} {args.tag} "
            header = f"OTA {args.gbl_filename} {chunk_index:4d} {len(pre_header):4d} {now()} {args.tag} "
            ota_message = header.encode("utf-8") + chunk_data
            data_string_info = (
                f": {chunk_data[0]:02x} {chunk_data[1]:02x} "
                f"---({chunk_data_len} bytes)--- {chunk_data[-2]:02x} {chunk_data[-1]:02x}"
            )
            data_range_info = (
                f": [{chunk_offset:8d}:{chunk_offset + chunk_data_len - 1:8d}]"
                f"/[{chunk_offset:08x}:{chunk_offset + chunk_data_len - 1:08x}]"
            )

            if should_send_chunk(chunk_index, selection):
                sequence = (base_sequence + chunk_index - 1) & 0xFFFF
                result = send_ota_message(sock,
                                          children,
                                          args.port,
                                          ota_message,
                                          args,
                                          scope_id,
                                          sequence,
                                          algo_selected,
                                          pan_size,
                                          chunk_index)
                print(header + data_string_info + data_range_info)
                if result != 0:
                    return result
                last_sent_data_len = chunk_data_len
                if not selection.only_mode:
                    time.sleep(args.interval_s)

            chunk_offset += chunk_data_len
            chunk_index += 1

            if selection.last_chunk and chunk_index > selection.last_chunk:
                break

        print(f"File {args.gbl_filename} sent in {chunk_index - 1} chunks; last chunk {last_sent_data_len} bytes")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
