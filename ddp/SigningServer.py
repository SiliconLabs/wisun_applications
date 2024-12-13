#!/usr/bin/env python3
# vim: set sw=2 expandtab:

import argparse
import logging
import tempfile
import os
import subprocess
import shutil
import shelve

from typing import Tuple, Union

certdirectory = 'certificates'
batch_number = 'Development'
certdb = f'{certdirectory}/certdb.txt'
root_cert = f'{certdirectory}/wisun_root_cert.pem'
root_key = f'{certdirectory}/wisun_root_key.pem'
batch_cert = f'{certdirectory}/wisun_batch_cert.pem'
batch_key = f'{certdirectory}/wisun_batch_key.pem'
device_cert = '{0}/wisun_device_{1}_cert.pem'
configdb = f'{certdirectory}/configdb'

device_ext = [
  '####################################################################',
  '[ v3_device ]',
  'keyUsage = critical,digitalSignature,keyAgreement',
  'extendedKeyUsage = critical,1.3.6.1.4.1.45605.1,clientAuth',
  'subjectAltName = critical,otherName:1.3.6.1.5.5.7.8.4;SEQ:id-on-hardwareModule',
  'authorityKeyIdentifier = keyid:always',
]

device_san = [
  '[ id-on-hardwareModule ]',
  'hwtype = OID:{0}',
  'hwserial = FORMAT:HEX,OCT:{1}'
]

def _process(cmd: Union[bytes, str], text=True) -> Union[bytes, str]:
  """Run command on system and return result.

  :param cmd: The command to run
  :param text: True for text mode (stdin, stdout and stderr are strings)
  :return: Command result
  """
  try:
    result = subprocess.run(cmd, capture_output=True, text=text, shell=True, check=True)
  except subprocess.CalledProcessError as e:
    logger.error(e.stderr)
    raise
  return result.stdout

def setup_pki(certdirectory):
  """Setup Public Key Infrastructure (PKI).

  :param certdirectory: Certificate directory
  """
  shutil.rmtree(certdirectory, ignore_errors=True)
  os.mkdir(certdirectory)
  db = open(certdb, 'w')
  db.close()

def save_oid(oid: str):
  """Save Object Identifier (OID) in PKI.

  :param oid: Device OID
  """
  with shelve.open(configdb) as db:
    db['oid'] = oid

def load_oid() -> str:
  """Get Object Identifier (OID) from PKI.

  :return: Device OID
  """
  with shelve.open(configdb) as db:
    return db['oid']

def generate_device_ext_config(oid: str, sn: str) -> str:
  """Prepare OpenSSL configuration for device certificate generation.

  :param oid: Device OID
  :param sn: Device serial number
  :return: Path to configuration file
  """
  req_handle, req_path = tempfile.mkstemp()
  ext = '\n'.join(device_ext) + '\n'
  os.write(req_handle, ext.encode('ascii'))
  ext = '\n'.join(device_san).format(oid, sn)
  os.write(req_handle, ext.encode('ascii'))
  os.close(req_handle)
  return req_path

def generate_req(config: str, output_key: str) -> str:
  """Generate Certificate Request (CSR) for specified config.

  :param config: String representing request configuration
  :param output_key: Filename to write the generated key
  :return: Path to generated file containing the CSR
  """
  req_handle, req_path = tempfile.mkstemp()
  os.close(req_handle)
  cmd = f'openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -keyout {output_key} -out {req_path} -config {config}'
  try:
    _process(cmd)
  except:
    os.remove(req_path)
    raise
  return req_path

def store_device_req(req: bytes) -> str:
  """Store a Certificate Request (CSR) in DER format.

  :param req: Request in bytes
  :return: Path to generated file containing the CSR
  """
  req_handle, req_path = tempfile.mkstemp()
  os.write(req_handle, req)
  os.close(req_handle)
  cmd = f'openssl req -in {req_path} -inform DER -out {req_path}'
  try:
    _process(cmd)
  except:
    os.remove(req_path)
    raise
  return req_path

def retrieve_cert(cert: str) -> bytes:
  """Get the key in DER format.

  :param cert: Path to file containing the key
  :return: Key in bytes
  """
  cmd = f'openssl x509 -in {cert} -outform DER'
  data = _process(cmd, False)
  return data

def generate_root(config: str, oem_name: str, oem_country: str, output_key: str, output_cert: str):
  """Generate a Wi-SUN root certificate.

  :param config: Path to OpenSSL configuration file
  :param oem_name: OEM name (e.g. Silicon Labs)
  :param oem_country: OEM country (e.g. FI, EU, US)
  :param output_key: Filename to write the generated key
  :param output_cert: Filename to write the generated certificate
  """
  dn = f'/CN={oem_name} {oem_country} WiSun Root/O={oem_name}/C={oem_country}'
  req = generate_req(config, output_key)
  cmd = f'openssl ca -batch -selfsign -rand_serial -keyfile {output_key} -in {req} -out {output_cert} -notext -extensions v3_root -policy wisun_ca_policy -config {config} -subj \"{dn}\"'
  try:
    _process(cmd)
  finally:
    os.remove(req)

def generate_batch(config: str, oem_name: str, oem_country: str, batch: str, sign_cert: str, sign_key: str, output_key: str, output_cert: str):
  """Generate a batch certificate.

  :param config: Path to OpenSSL configuration file
  :param oem_name: OEM name (e.g. Silicon Labs)
  :param oem_country: OEM country (e.g. FI, EU, US)
  :param batch: Batch identifier
  :param sign_cert: Path to file containing the CA certificate
  :param sign_key: Path to file containing the CA private key
  :param output_key: Filename to write the generated key
  :param output_cert: Filename to write the generated certificate
  """
  dn = f'/CN=Batch ({batch})/O={oem_name}/C={oem_country}'
  req = generate_req(config, output_key)
  cmd = f'openssl ca -batch -rand_serial -cert {sign_cert} -keyfile {sign_key} -in {req} -out {output_cert} -notext -extensions v3_batch -policy wisun_ca_policy -config {config} -subj \"{dn}\"'
  try:
    _process(cmd)
  finally:
    os.remove(req)

def generate_device(config: str, oid: str, sn: str, csr: bytes, sign_cert: str, sign_key: str, output_cert: str):
  """Generate a device certificate
  :param config: Path to OpenSSL configuration file
  :param oid: Device OID
  :param sn: Device serial number
  :param csr: Certificate request (CSR)
  :param sign_cert: Path to file containing the CA certificate
  :param sign_key: Path to file containing the CA private key
  :param output_cert: Filename to write the generated certificate
  """
  req = store_device_req(csr)
  ext = generate_device_ext_config(oid, sn)
  cmd = f'openssl ca -batch -rand_serial -cert {sign_cert} -keyfile {sign_key} -in {req} -out {output_cert} -notext -extfile {ext} -extensions v3_device -policy wisun_device_policy -config {config}'
  try:
    _process(cmd)
  finally:
    os.remove(req)
    os.remove(ext)

def SetupCerts(oem_name: str, oem_country: str, oid: str, config: str):
  """Prepares environment for device certificate generation.

  :param oem_name: OEM name (e.g. Silicon Labs)
  :param oem_country: OEM country (e.g. FI, EU, US)
  :param oid: Device OID
  :param config: Path to OpenSSL configuration file
  """
  logger.debug(f"Setting up local PKI for {oem_name} {oem_country}")
  setup_pki(certdirectory)
  logger.debug(f"Storing device OID {oid}")
  save_oid(oid)
  logger.debug("Generating local root certificate")
  generate_root(config, oem_name, oem_country, root_key, root_cert)
  logger.debug("Generating local batch certificate")
  generate_batch(config, oem_name, oem_country, batch_number, root_cert, root_key, batch_key, batch_cert)

def GetCerts(csr: bytes, sn: str, config: str) -> Tuple[bytes, bytes, bytes]:
  """Generate and return device, batch and root certificates.

  Use local PKI, device OID and certificates generated during setup.
  :param csr: Certificate Request (CSR)
  :param sn: Device serial number
  :param config: Path to OpenSSL configuration file
  :return: Device, batch and root certificates in DER format
  """
  oid = load_oid()
  logger.debug(f"Utilizing device OID {oid}")
  logger.debug(f"Generating local device certificate for {sn}")
  output_cert = device_cert.format(certdirectory, sn.lower())
  generate_device(config, oid, sn, csr, batch_cert, batch_key, output_cert)
  logger.debug(f"Retrieving local device certificate")
  device = retrieve_cert(output_cert)
  logger.debug(f"Retrieving local batch certificate")
  batch = retrieve_cert(batch_cert)
  logger.debug(f"Retrieving local root certificate")
  root = retrieve_cert(root_cert)
  return device, batch, root

def GetCertificationCerts(csr: bytes, sn: str, config: str) -> Tuple[bytes, bytes, bytes]:
  """Generate and return device, batch and root certificates for certification process.

  Use local PKI, device OID and certificates generated during setup.
  :param csr: Certificate Request (CSR)
  :param sn: Device serial number
  :param config: Path to OpenSSL configuration file
  :return: Device, batch and root certificates in DER format
  """
  raise NotImplementedError

def _setup_certs(args):
  SetupCerts(args.co, args.cu, args.oid, args.config)

def _get_certs(args):
  device, batch, root = GetCerts(bytearray.fromhex(args.csr), args.sn, args.config)
  logger.info('OK {0} {1} {2}'.format(device.hex(), batch.hex(), root.hex()))

def _get_certification_certs(args):
  GetCertificationCerts(args.csr, args.sn, args.config)

logger = logging.getLogger('SigningServer')
logger.setLevel(logging.DEBUG)
ch = logging.StreamHandler()
ch.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
logger.addHandler(ch)

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='Script for managing Wi-SUN certificates.')
  subparsers = parser.add_subparsers(help='sub-command help')

  # SetupCerts
  parser_setup = subparsers.add_parser('SetupCerts', help='Setup local PKI')
  parser_setup.add_argument('--co', action='store', required=True, help='OEM company name')
  parser_setup.add_argument('--cu', action='store', required=True, help='OEM country name')
  parser_setup.add_argument('--oid', action='store', required=True, help='Device OID')
  parser_setup.add_argument('--config', action='store', default='openssl.conf', help='OpenSSL configuration file (default: openssl.conf)')
  parser_setup.set_defaults(func=_setup_certs)

  # GetCerts
  parser_cert = subparsers.add_parser('GetCerts', help='Generate a local device certificate')
  parser_cert.add_argument('--csr', action='store', required=True, help='Device CSR')
  parser_cert.add_argument('--sn', action='store', required=True, help='Device serial number')
  parser_cert.add_argument('--config', action='store', default='openssl.conf', help='OpenSSL configuration file (default: openssl.conf)')
  parser_cert.set_defaults(func=_get_certs)

  # GetCertificationCerts
  parser_certif = subparsers.add_parser('GetCertificationCerts', help='Generate a Wi-SUN certification device certificate')
  parser_certif.add_argument('--csr', action='store', required=True, help='Device CSR')
  parser_certif.add_argument('--sn', action='store', required=True, help='Device serial number')
  parser_certif.add_argument('--config', action='store', default='openssl.conf', help='OpenSSL configuration file (default: openssl.conf)')
  parser_certif.set_defaults(func=_get_certification_certs)

  args = parser.parse_args()
  args.func(args)
