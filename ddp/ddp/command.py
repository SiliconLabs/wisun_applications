import struct

class CommandList:
  """List of DDP commands."""
  WRITE_NVM = 1
  INJECT_KEY = 2
  GENERATE_KEY = 3
  INITIALIZE_NVM = 7

class Command(bytes):
  def __new__(cls, cmd: CommandList, body: bytes) -> "Command":
    """Input structure of DDP command.

    :param cmd: Command ID
    :param body: Command body
    """
    return super().__new__(cls, struct.pack('<HH', int(cmd), len(body)) + body)

class InitializeNvm(Command):
  def __new__(cls, base_addr: int, nvm3_inst_size: int) -> "InitializeNvm":
    """Input structure of DDP command for initializing NVM.

    :param base_addr: Address in flash of the NVM3 instance
    :param nvm3_inst_size: Size of the NVM3 instance in bytes
    """
    return super().__new__(
      cls,
      CommandList.INITIALIZE_NVM,
      struct.pack('<LL', base_addr,  nvm3_inst_size)
    )

class WriteNvm(Command):
  def __new__(cls, obj_key: int, obj_data: bytes) -> "WriteNvm":
    """Input structure of DDP command for writing an NVM object.

    :param obj_key: NVM Object Key
    :param obj_data: Data to write
    """
    return super().__new__(
      cls,
      CommandList.WRITE_NVM,
      struct.pack('<LH', obj_key, len(obj_data)) + obj_data
    )

class InjectKey(Command):
  def __new__(cls, ka_lifetime: int, ka_location: int, ka_usage_flags: int, ka_bits: int, ka_algo: int, ka_type: int, key_id: int, key: bytes) -> "InjectKey":
    """Input structure of DDP command for injecting a PSA Crypto key.

    :param ka_lifetime: Lifetime of the key as psa_key_type_t
    :param ka_location: Location of the key as psa_key_location_t
    :param ka_usage_flags: Permitted usage of the key as psa_key_usage_t
    :param ka_bits: Length of key in bits
    :param ka_algo: Permitted algorithms of the key as psa_algorithm_t
    :param ka_type: Type of the key as psa_key_type_t
    :param key_id: PSA Key ID
    :param key: Injected key
    """
    return super().__new__(
      cls,
      CommandList.INJECT_KEY,
      struct.pack('<LLLLLHLL', ka_lifetime, ka_location, ka_usage_flags, ka_bits, ka_algo, ka_type, key_id, len(key)) + key
    )

class GenerateKey(Command):
  def __new__(cls, ka_lifetime: int, ka_location: int, ka_usage_flags: int, ka_bits: int, ka_algo: int, ka_type: int, key_id: int) -> "GenerateKey":
    """Input structure of DDP command for generating a PSA Crypto key.

    :param ka_lifetime: Lifetime of the key as psa_key_type_t
    :param ka_location: Location of the key as psa_key_location_t
    :param ka_usage_flags: Permitted usage of the key as psa_key_usage_t
    :param ka_bits: Length of key in bits
    :param ka_algo: Permitted algorithms of the key as psa_algorithm_t
    :param ka_type: Type of the key as psa_key_type_t
    :param key_id: PSA Key ID
    """
    return super().__new__(
      cls,
      CommandList.GENERATE_KEY,
      struct.pack('<LLLLLHL', ka_lifetime, ka_location, ka_usage_flags, ka_bits, ka_algo, ka_type, key_id)
    )
