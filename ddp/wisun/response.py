# !/usr/bin/env python3

import ddp.response
import struct

class GenerateCsr(ddp.response.Response):
  def __init__(self, data):
    """Output structure of DDP command for generating a Wi-SUN CSR.

    :param data: Response data
    """
    super().__init__(data)
    self.csr = None
    if self.status == 0:
      length, = struct.unpack('<L', self.body[:4])
      self.csr = self.body[4:4+length]

class GenerateKeyPair(ddp.response.Response):
  def __init__(self, data):
    """Output structure of DDP command for generating a Wi-SUN device key pair.

    :param data: Response data
    """
    super().__init__(data)
    self.key = None
    if self.status == 0:
      length, = struct.unpack('<L', self.body[:4])
      self.key = self.body[4:4+length]

class InjectKey(ddp.response.Response):
  """Output structure of DDP command for injecting a Wi-SUN device key pair."""
  pass
