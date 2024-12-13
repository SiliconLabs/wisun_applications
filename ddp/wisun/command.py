# !/usr/bin/env python3

import ddp.command
import struct

class CommandList(object):
  """List of Wi-SUN related DDP commands."""
  GENERATE_CSR = 200
  GENERATE_KEY_PAIR = 201
  INJECT_KEY = 202

class GenerateCsr(ddp.command.Command):
  def __new__(cls, key_id: int) -> "GenerateCsr":
    """Input structure of DDP command for generating a Wi-SUN CSR.

    :param key_id: PSA Key ID
    """
    return super().__new__(
      cls,
      CommandList.GENERATE_CSR,
      struct.pack('<L', key_id)
    )

class GenerateKeyPair(ddp.command.Command):
  def __new__(cls, key_id: int) -> "GenerateKeyPair":
    """Input structure of DDP command for generating a Wi-SUN device key pair.

    :param key_id: PSA Key ID
    """
    return super().__new__(
      cls,
      CommandList.GENERATE_KEY_PAIR,
      struct.pack('<L', key_id)
    )

class InjectKey(ddp.command.Command):
  def __new__(cls, key_id: int, key: bytes) -> "InjectKey":
    """Input structure of DDP command for injecting a Wi-SUN device key pair.

    :param key_id: PSA Key ID
    :param key: Injected key
    """
    return super().__new__(
      cls,
      CommandList.INJECT_KEY,
      struct.pack('<LL', key_id, len(key)) + key
    )
