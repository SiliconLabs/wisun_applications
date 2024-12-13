import struct

class Response:
  def __init__(self, data: bytes):
    """Output structure of DDP command.

    :param data: Response data
    """
    self.status, length = struct.unpack('<LH', data[:6])
    self.body = data[6:6+length]

class InitializeNvm(Response):
  """Output structure of DDP command for initializing NVM."""
  pass

class WriteNvm(Response):
  """Output structure of DDP command for writing an NVM object."""
  pass

class InjectKey(Response):
  """Output structure of DDP command for injecting a PSA Crypto key."""
  pass

class GenerateKey(Response):
  def __init__(self, data: bytes):
    """Output structure of DDP command for generating a PSA Crypto key.

    :param data: Response data
    """
    super().__init__(data)
    self.key = None
    if self.status == 0:
      length, = struct.unpack('<L', self.body[:4])
      self.key = self.body[4:4+length]
