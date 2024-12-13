# !/usr/bin/env python3

import pylink
import socket
import time

class SerialWire:

  def __init__(self, chip_name: str, serial_no: int = None, hostname: str = None, xml_path: str = None):
    """Communication interface to device.

    :param chip_name: Chip family (e.g. xg12, xg25)
    :param serial_no: J-Link serial number to use USB interface
    :param hostname: Hostname to use Ethernet interface
    :param xml_path: Path to JLinkDevicesXML folder
    """
    self.jlink = pylink.JLink()
    self.chip_name = chip_name
    self.serial_no = serial_no
    self.hostname = hostname
    if xml_path:
      self.jlink.exec_command(f"JLinkDevicesXMLPath = \"{xml_path}\"")

  @property
  def is_connected(self) -> bool:
    """Indicate device connection is established.

    :return: True if connected, False otherwise
    """
    return self.jlink.connected()

  def connect(self):
    """Connect to device using serial_no or hostname."""
    if self.serial_no:
      self.jlink.open(serial_no=self.serial_no)
    elif self.hostname:
      ip_addr = socket.gethostbyname(self.hostname)
      self.jlink.open(ip_addr=f"{ip_addr}:19020")
    else:
      raise ValueError("No J-Link serial number or hostname specified")
    self.jlink.set_tif(interface=pylink.JLinkInterfaces.SWD)
    self.jlink.connect(chip_name=self.chip_name, speed="auto", verbose=True)

  def reset_and_halt(self):
    """Reset and halt the device."""
    self.jlink.reset(halt=True)

  def reset(self):
    """Reset the device."""
    self.jlink.reset(halt=False)

  def rtt_start(self, block_address: int = None, timeout: float = 10):
    """Start RTT processing.

    :param block_address: Optional RTT block address
    :param timeout: Time to wait for RTT processing started in seconds
    """
    self.jlink.rtt_start(block_address)
    start = now = time.time()
    while now < start + timeout:
      try:
        self.jlink.rtt_get_buf_descriptor(0, False)
      except pylink.errors.JLinkRTTException:
        pass
      else:
        return
      now = time.time()
    raise TimeoutError

  def rtt_stop(self):
    """Stops RTT."""
    self.jlink.rtt_stop()

  def rtt_send(self, data: bytes, timeout: float = 10):
    """Send data to RTT buffer.

    :param data: Bytes to write to RTT buffer.
    :param timeout: Maximum time to wait for data to be written in seconds
    """
    nb_sent = 0
    start = now = time.time()
    while nb_sent == 0 and now < start + timeout:
      nb_sent = self.jlink.rtt_write(0, data)
      now = time.time()
    if nb_sent == 0:
      raise TimeoutError

  def rtt_receive(self, timeout: float = 10) -> bytes:
    """Read data from RTT buffer.

    :param timeout: Maximum time to wait for data to be received in seconds
    :return: Data received
    """
    data = bytes()
    start = now = time.time()
    while len(data) == 0 and now < start + timeout:
      data = self.jlink.rtt_read(0, 1024)
      now = time.time()
    if len(data) == 0:
      raise TimeoutError
    return bytes(data)

  def close(self):
    """Close the connection."""
    self.jlink.close()

  def run_application(self, ram_addr: int, img: bytes):
    """Flash and run the provided firmware in device's RAM.

    :param ram_addr: Address on RAM memory to flash the firmware
    :param img: Firmware image
    """
    self.jlink.memory_write8(addr=ram_addr, data=list(img))
    sp, pc = self.jlink.memory_read32(addr=ram_addr, num_words=2)
    self.jlink.register_write(reg_index="R13 (SP)", value=sp)
    self.jlink.register_write(reg_index="R15 (PC)", value=pc)
    self.jlink.restart(num_instructions=0, skip_breakpoints=False)

  def get_mac_address(self) -> str:
    """Get the device's builtin EUI-64.

    :return: EUI-64 in String format without ':'
    """
    l, h = self.jlink.memory_read32(0x0FE08000 + 0x48, 2)
    return f"{h * 0x100000000 + l:016x}"
