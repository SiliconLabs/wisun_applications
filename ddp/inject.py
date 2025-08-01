#!/usr/bin/env python3
# vim: set sw=2 expandtab:

import sys
import argparse
import logging
from ddp.commander import *
import wisun.common
import wisun.command
import wisun.response
import ddp.command
import ddp.response
from ddp.rtt import SerialWire
import SigningServer
import os
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization

if __name__ == '__main__':
  logger = logging.getLogger('provision')
  logger.setLevel(logging.DEBUG)
  ch = logging.StreamHandler()
  ch.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
  logger.addHandler(ch)

  parser = argparse.ArgumentParser(description='Script for performing Wi-SUN provisioning.')
  parser.add_argument('--soc', action='store', required=True, help='SoC type')
  parser.add_argument('--prov_img', action='store', required=True, help='Input file for provisiong application')
  parser.add_argument('--jlink_ser', action='store', default=None, help='Serial number of J-Link adapter')
  parser.add_argument('--jlink_host', action='store', default=None, help='Host name or IP address of J-Link adapter')
  parser.add_argument('--device_key', action='store', default=None, help='Device private key')
  parser.add_argument('--device_cert', action='store', default=None, help='Device private certificate')
  parser.add_argument('--trusted_cert', action='append', default=None, help='List of trusted CA certificates')

  args = parser.parse_args()

  try:
    soc = wisun.common.socs[args.soc]
  except KeyError:
    print(f'{parser.prog}: error: {args.soc} is not a supported SoC type', file=sys.stderr)
    exit(1)

  with open(args.prov_img, 'rb') as f:
    provisioning_app = f.read()

  device_key = None
  device_cert = None
  trusted_certs = []

  if args.device_key:
    with open(args.device_key, 'rb') as f:
      key_data = f.read()
      try:
        device_key_data = serialization.load_pem_private_key(key_data, None, default_backend())
      except ValueError:
        device_key_data = serialization.load_der_private_key(key_data, None, default_backend())
      device_key = device_key_data.private_numbers().private_value.to_bytes(32, 'big')
  if args.device_cert:
    with open(args.device_cert, 'rb') as f:
      device_cert = f.read()
  if args.trusted_cert:
    for iter in args.trusted_cert:
      with open(iter, 'rb') as f:
        trusted_cert = f.read()
        trusted_certs.append(trusted_cert)

  # Connect to the device
  logger.info("Opening SerialWire connection to the device")
  jlink_xml = os.path.join(os.path.dirname(__file__), "jlink/JLinkDevices.xml")
  sw = SerialWire(soc['device'], args.jlink_ser, args.jlink_host, jlink_xml)
  sw.connect()
  sw.reset_and_halt()
  logger.info("Connection opened")

  try:
    # Inject and run provisioning application
    logger.info("Injecting provisioning application")
    ram_addr = soc['ramstartaddress']
    sw.run_application(ram_addr, provisioning_app)
    sw.rtt_start()
    logger.info("Provisioning application running")

    # Initialize NVM
    logger.info("Initializing NVM")
    tx = ddp.command.InitializeNvm(soc['nvm3inststartaddress'], soc['nvm3instsize'])
    sw.rtt_send(tx)
    rx = sw.rtt_receive()
    resp = ddp.response.InitializeNvm(rx)
    assert resp.status == 0, f"Failure during NVM initialization ({resp.status})"
    logger.info("NVM initialized")

    if device_key:
      # Inject private key as PSA Key ID 0x100
      logger.info("Injecting device private key")
      tx = wisun.command.InjectKey(0x100, device_key)
      sw.rtt_send(tx)
      rx = sw.rtt_receive()
      resp = ddp.response.InjectKey(rx)
      assert resp.status == 0, f"Failure injecting device private key ({resp.status})"
      logger.info("Device private key injected")

    if device_cert:
      # Write device certificate to NVM3 Object ID 0x100
      logger.info("Saving device certificate to NVM")
      tx = ddp.command.WriteNvm(0x100, device_cert)
      sw.rtt_send(tx)
      rx = sw.rtt_receive()
      resp = ddp.response.WriteNvm(rx)
      assert resp.status == 0, f"Failure saving device certificate to NVM ({resp.status})"
      logger.info("Device certificate saved")

    key_id = 1
    for iter in trusted_certs:
      # Write trusted CA certificate to NVM3 Object ID (0x100 + key_id)
      logger.info(f"Saving trusted CA certificate #{key_id} to NVM")
      tx = ddp.command.WriteNvm(0x100 + key_id, iter)
      sw.rtt_send(tx)
      rx = sw.rtt_receive()
      resp = ddp.response.WriteNvm(rx)
      assert resp.status == 0, f"Failure saving trusted CA certificate #{key_id} to NVM ({resp.status})"
      logger.info(f"Trusted CA certificate #{key_id} saved")
      key_id += 1

  finally:
    sw.rtt_stop()
    sw.reset()
    sw.close()

  logger.info("Finished")
