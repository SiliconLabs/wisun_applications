# !/usr/bin/env python3

import subprocess
import re
import socket
import logging

from typing import Dict

# commander output handling
COMMANDER_EXP_MSG_OK_PATTERN = r"DONE"
COMMANDER_EXP_MSG_ERR_PATTERN = r"ERROR"

# NVM3 content file line structure
NVM3_CONTENT_LINE_STRUCT = "{0}:OBJ:{1}"

# parameterizable commander commands
COMMANDER_CMD_NVM3_SET = ""
COMMANDER_CMD_CONVERT = ""

class Commander:
  """Python wrapper for Simplicity Commander."""

  def __init__(self, jlink_ser: int = None, jlink_host: str = None):
    """Initialize the Simplicity Commander wrapper.

        :param jlink_ser: J-Link serial number to use USB interface
    :param jlink_host: Hostname to use Ethernet interface
    """
    if jlink_host:
      ip = socket.gethostbyname(jlink_host)
      self.jlink_adapter = f'--ip {ip}'
    else:
      self.jlink_adapter = f'--serialno {jlink_ser}'
    self.logger = logging.getLogger('Commander')
    self.logger.setLevel(logging.DEBUG)
    ch = logging.StreamHandler()
    ch.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
    self.logger.addHandler(ch)

  def _process(self, cmd: str) -> str:
    """Run command on system and return result.

    :param cmd: The command to run
    :return: Command result
    """
    try:
      result = subprocess.run(cmd, capture_output=True, text=True, shell=True, check=True)
    except subprocess.CalledProcessError as e:
      self.logger.error(e.stderr)
      raise
    if re.search(COMMANDER_EXP_MSG_ERR_PATTERN, result.stdout):
      raise RuntimeError(result.stdout)
    return result.stdout

  def masserase(self):
    """Execute a device mass erase, clearing the main flash."""
    cmd = f'commander device masserase {self.jlink_adapter}'
    self._process(cmd)

  def flash(self, filename):
    """Write one file to the target flash.

    :param filename: Path to file to write
    """
    cmd = f'commander flash {filename} {self.jlink_adapter}'
    self._process(cmd)

  def get_mac_address(self) -> str:
    """Get the device's builtin EUI-64.

    :return: EUI-64 in String format without ':'
    """
    cmd = f'commander device info {self.jlink_adapter}'
    info = self._process(cmd).splitlines()
    for line in info:
      tv = line.split(":")
      if tv[0].strip() == 'Unique ID':
        return tv[1].strip()
    raise ValueError("No MAC address found")

  def create_nvm3_initfile(self, nvm3inststartaddress, nvm3instsize, device, outfile):
    """Create an image file containing a blank NVM3 area with the given size and location.

    :param nvm3inststartaddress: Address where the NVM3 area will be placed
    :param nvm3instsize: Size of the NVM3 area that will be initialized in bytes
    :param device: The device, device family or platform to target (e.g. "EFR32MG1P233F256GM48", "EFR32MG", "EFR32","EFR32F256")
    :param outfile: The file to write output to
    """
    cmd = f'commander nvm3 initfile --address {nvm3inststartaddress} --size {nvm3instsize} --device {device} --outfile {outfile}'
    self._process(cmd)

  def set_nvm3(self, initfile, nvm3file, outfile):
    """Set the value of one or more NVM3 objects in a file.

    The file must already contain an NVM3 area created with `create_nvm3_initfile`.
    :param initfile:
    :param nvm3file: Path to a file containing a list of NVM3 items
    :param outfile: The file to write output to
    """
    cmd = f'commander nvm3 set {initfile} --nvm3file {nvm3file} --outfile {outfile}'
    self._process(cmd)

  def convert(self, file1, file2, outfile):
    """Combine files into one unique file.

    :param file1: Path to first file
    :param file2: Path to second file
    :param outfile: The file to write output to
    """
    arg1 = "" if file2 == None else file2 # omit file2 if not provided
    cmd = f'commander convert {file1} {arg1} --outfile {outfile}'
    self._process(cmd)

  def generate_nvm3_content(self, kv_set: Dict[int, int]) -> str:
    """Generate file content with NVM3 objects.

    :param kv_set: Dictionary containing NVM3 objects
    :return: String with NVM3 objects separated by '\n'
    """
    objs = list()
    for k, v in kv_set.items():
      objs.append(NVM3_CONTENT_LINE_STRUCT.format("0x%04x" % k, v))
      objs.append("\n")
    objs.pop()
    return "".join(objs)
