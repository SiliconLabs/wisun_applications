#!/usr/bin/env python3
"""Send PROPAG EMERGENCY frames from a Linux Wi-SUN Border Router.

The script parses `wsbrd_cli status`, sends only to the Border Router's direct
children, and optionally waits for aggregated PROPAG CONF replies.
"""

from __future__ import annotations

import argparse
import ipaddress
import shlex
import socket
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path


PROPAGATION_PORT = 7778
COAP_PORT = 5683
COAP_TIMEOUT_S = 3
DISCRIMINATOR = "EMERGENCY"
TIMESTAMP_FORMAT = "%Y-%m-%d_%H:%M:%S"
LINKLOCAL_ALL_NODES = "ff02::1"
PROPAGATION_FRAME_MAX_BYTES = 1232


@dataclass
class Confirmation:
    sender: str
    sequence: int
    algo_selected: int
    pan_size: int
    payload: str
    count: int
    option: str


def ipv6_or_none(text: str) -> str | None:
    try:
        return str(ipaddress.IPv6Address(text))
    except ValueError:
        return None


def ipv6_last4(addr: str) -> str:
    return ipaddress.IPv6Address(addr).packed[-2:].hex()


def timestamp() -> str:
    return datetime.now().strftime(TIMESTAMP_FORMAT)


def log(message: str) -> None:
    print(f"[{timestamp()}] {message}")


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


def log_child_run_time(child: str) -> None:
    uri = f"coap://[{child}]:{COAP_PORT}/time/run_time"
    command = [
        "coap-client",
        "-m",
        "get",
        "-N",
        "-B",
        str(COAP_TIMEOUT_S),
        "-t",
        "text/plain",
        uri,
    ]
    start_time = time.monotonic()

    log(f"GET run_time from {child} ({ipv6_last4(child)}): {uri}")
    try:
        result = subprocess.run(
            command,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=COAP_TIMEOUT_S + 2,
        )
    except FileNotFoundError:
        log("run_time GET failed: coap-client command not found")
        return
    except subprocess.TimeoutExpired:
        log(f"run_time GET timeout after {COAP_TIMEOUT_S} s")
        return

    elapsed_ms = (time.monotonic() - start_time) * 1000.0
    run_time = result.stdout.strip()
    if result.returncode == 0:
        log(f"run_time from {child} ({ipv6_last4(child)}): {run_time or '<empty>'} ({elapsed_ms:.1f} ms)")
    else:
        error = result.stderr.strip() or result.stdout.strip() or f"returncode={result.returncode}"
        log(f"run_time GET failed from {child} ({ipv6_last4(child)}): {error} ({elapsed_ms:.1f} ms)")


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


def build_frame(confirmable: bool,
                payload: str,
                timeout_ms: int,
                sequence: int,
                algo_selected: int,
                pan_size: int) -> bytes:
    payload_bytes = payload.encode("utf-8")
    if confirmable:
        frame = (
            f"PROPAG IND_CON {sequence} {algo_selected} {pan_size} "
            f"{DISCRIMINATOR} {len(payload_bytes)} {timeout_ms} "
        ).encode("ascii") + payload_bytes
    else:
        frame = (
            f"PROPAG IND_NON {sequence} {algo_selected} {pan_size} "
            f"{DISCRIMINATOR} {len(payload_bytes)} "
        ).encode("ascii") + payload_bytes

    if len(frame) > PROPAGATION_FRAME_MAX_BYTES:
        raise ValueError(
            f"PROPAG frame is {len(frame)} bytes, max is {PROPAGATION_FRAME_MAX_BYTES}"
        )
    return frame


def parse_confirmation(data: bytes, sender: str, expected_sequence: int) -> Confirmation | None:
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
    return Confirmation(
        sender=sender,
        sequence=sequence,
        algo_selected=algo_selected,
        pan_size=pan_size,
        payload=parts[6],
        count=count,
        option=option,
    )


def missing_tokens_from_option(option: str) -> set[str]:
    missing: set[str] = set()
    for chunk in option.replace(";", " ").split():
        if chunk.startswith("MISS="):
            chunk = chunk[len("MISS=") :]
        for token in chunk.split(","):
            token = token.strip()
            if token:
                missing.add(token)
    return missing


def make_socket(port: int, timeout_s: float | None) -> socket.socket:
    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("::", port))
    sock.settimeout(timeout_s)
    return sock


def send_to_children(sock: socket.socket, children: list[str], port: int, frame: bytes) -> None:
    for child in children:
        sock.sendto(frame, (child, port, 0, 0))
        log(f"sent to {child} ({ipv6_last4(child)}): {frame.decode('utf-8', errors='replace')}")
        time.sleep(0.2)


def send_linklocal_broadcast(sock: socket.socket, port: int, frame: bytes, scope_id: int) -> None:
    if scope_id != 0:
        sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_IF, scope_id)
    sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_HOPS, 1)
    sock.sendto(frame, (LINKLOCAL_ALL_NODES, port, 0, scope_id))
    scope_text = f" scope_id={scope_id}" if scope_id != 0 else ""
    log(f"sent link-local broadcast to [{LINKLOCAL_ALL_NODES}]:{port}{scope_text}: {frame.decode('utf-8', errors='replace')}")


def expected_child_from_sender(sender: str, children: list[str]) -> str | None:
    if sender in children:
        return sender

    sender_tag = ipv6_last4(sender)
    matches = [child for child in children if ipv6_last4(child) == sender_tag]
    if len(matches) == 1:
        return matches[0]
    return None


def receive_confirmations(sock: socket.socket,
                          children: list[str],
                          timeout_ms: int,
                          start_time: float,
                          sequence: int) -> int:
    confirmations: dict[str, Confirmation] = {}
    reported_missing: set[str] = set()
    deadline = time.monotonic() + (timeout_ms*2 / 1000.0)
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
        rx_text = data.decode("ascii", errors="replace").strip()
        log(f"received UDP from {sender} ({ipv6_last4(sender)}): {rx_text}")
        confirmation = parse_confirmation(data, sender, sequence)
        if confirmation is None:
            print(f"ignored UDP from {addr[0]}: {rx_text}")
            continue

        expected_child = expected_child_from_sender(sender, children)
        if expected_child is None:
            print(f"ignored CONF from non-direct child {sender}: {rx_text}")
            continue

        if expected_child in confirmations:
            print(f"ignored duplicate CONF from {sender}")
            continue

        confirmations[expected_child] = confirmation
        last_confirmation_time = time.monotonic()
        reported_missing.update(missing_tokens_from_option(confirmation.option))
        print(
            f"CONF from {sender} ({ipv6_last4(sender)}): "
            f"seq={confirmation.sequence} algo={confirmation.algo_selected} pan={confirmation.pan_size} "
            f"payload={confirmation.payload} count={confirmation.count} option={confirmation.option or '-'}"
        )

    no_response = set(children) - confirmations.keys()
    local_missing = {ipv6_last4(addr) for addr in no_response}
    all_missing = sorted(local_missing | reported_missing)
    total_count = sum(conf.count for conf in confirmations.values())
    elapsed_end = last_confirmation_time if len(confirmations) == len(children) else time.monotonic()
    elapsed_ms = (elapsed_end - start_time) * 1000.0

    print(f"direct child confirmations: {len(confirmations)}/{len(children)}")
    print(f"direct child confirmation elapsed: {elapsed_ms:.1f} ms")
    print(f"aggregated confirmed nodes below BR: {total_count}")
    print(f"missing tags: {','.join(all_missing) if all_missing else 'none'}")

    return 0 if not all_missing and len(confirmations) == len(children) else 2


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send PROPAG EMERGENCY frames to direct children from wsbrd_cli topology."
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--CON", dest="confirmable", action="store_true", help="send IND_CON and wait for CONF")
    mode.add_argument("--NON", dest="confirmable", action="store_false", help="send IND_NON")
    parser.set_defaults(confirmable=False)

    parser.add_argument("--timeout", type=int, default=10000, help="timeout in ms for IND_CON and CONF wait")
    parser.add_argument(
        "--payload",
        default=None,
        help="UTF-8 payload; default is current timestamp yyyy-mm-dd_hh:mn:ss",
    )
    parser.add_argument("--port", type=int, default=PROPAGATION_PORT, help="UDP propagation port")
    parser.add_argument("--sequence", type=int, help="PROPAG sequence number; default is current time modulo 65536")
    parser.add_argument(
        "--algo",
        "--algo-selected",
        dest="algo_selected",
        type=int,
        choices=(0, 1),
        default=None,
        help="PROPAG algorithm selector; default is 1 with --broadcast_linklocal, otherwise 0",
    )
    parser.add_argument("--status-command", default="wsbrd_cli status", help="command used to read BR topology")
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
    parser.add_argument(
        "--interface",
        help="outgoing interface name or index for --broadcast_linklocal, for example tun0 or 12",
    )
    parser.add_argument("--dry-run", action="store_true", help="parse and print targets without sending UDP")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.timeout < 0:
        raise ValueError("--timeout must be >= 0")
    if args.sequence is not None and not 0 <= args.sequence <= 0xFFFF:
        raise ValueError("--sequence must be between 0 and 65535")

    status_text = read_status(args)
    br_addr, children, branch_sizes, pan_size = parse_topology(status_text)
    if args.big_branch_first and not args.broadcast_linklocal:
        children = sorted(children, key=lambda addr: branch_sizes.get(addr, 0), reverse=True)

    payload = args.payload if args.payload is not None else timestamp()
    sequence = args.sequence if args.sequence is not None else int(time.time() * 1000.0) & 0xFFFF
    algo_selected = args.algo_selected if args.algo_selected is not None else (1 if args.broadcast_linklocal else 0)
    frame = build_frame(args.confirmable, payload, args.timeout, sequence, algo_selected, pan_size)
    scope_id = interface_scope_id(args.interface)

    print(f"BR: {br_addr}")
    print(f"PAN size: {pan_size}")
    print(f"PROPAG sequence: {sequence}")
    print(f"PROPAG algo_selected: {algo_selected}")
    print(
        "direct children:",
        ", ".join(
            f"{addr} ({ipv6_last4(addr)}, branch={branch_sizes.get(addr, 0)})"
            for addr in children
        ) or "none",
    )
    if args.broadcast_linklocal:
        print(f"send mode: link-local broadcast {LINKLOCAL_ALL_NODES}")
        if scope_id != 0:
            print(f"broadcast interface scope_id: {scope_id}")
    elif args.big_branch_first:
        print("send order: biggest branch first")
    print(f"frame: {frame.decode('utf-8', errors='replace')}")

    if not args.dry_run and children:
        log_child_run_time(children[0])

    if args.dry_run or not children:
        return 0

    with make_socket(args.port, None if args.confirmable else 0.0) as sock:
        start_time = time.monotonic()
        if args.broadcast_linklocal:
            send_linklocal_broadcast(sock, args.port, frame, scope_id)
        else:
            send_to_children(sock, children, args.port, frame)
        if args.confirmable:
            return receive_confirmations(sock, children, args.timeout, start_time, sequence)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
