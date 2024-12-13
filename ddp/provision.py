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

if __name__ == '__main__':
  logger = logging.getLogger('provision')
  logger.setLevel(logging.DEBUG)
  ch = logging.StreamHandler()
  ch.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
  logger.addHandler(ch)

  parser = argparse.ArgumentParser(description='Script for performing Wi-SUN provisioning.')
  parser.add_argument('--soc', action='store', required=True, help='SoC type')
  parser.add_argument('--init_img', action='store', default=None, help='Input file for initialization data')
  parser.add_argument('--prov_img', action='store', required=True, help='Input file for provisiong application')
  parser.add_argument('--jlink_ser', action='store', default=None, help='Serial number of J-Link adapter')
  parser.add_argument('--jlink_host', action='store', default=None, help='Host name or IP address of J-Link adapter')
  parser.add_argument('--app', action='store', default=None, help='Input file for application')
  parser.add_argument('--nvm3', action='store_true', default=False, help='?')
  parser.add_argument('--certification', action='store_true', default=False, help='Certification mode')
  parser.add_argument('--cpms', action='store_true', default=False, help='CPMS mode')
  parser.add_argument('--oid', action='store', default=None, help='Product OID when CPMS mode is set')
  parser.add_argument('--config', action='store', default='openssl.conf', help='OpenSSL configuration file (default: openssl.conf)')

  args = parser.parse_args()

  if args.cpms and not args.oid:
    print(f'{parser.prog}: error: --oid is required when --cpms is set', file=sys.stderr)
    exit(1)

  try:
    soc = wisun.common.socs[args.soc]
  except KeyError:
    print(f'{parser.prog}: error: {args.soc} is not a supported SoC type', file=sys.stderr)
    exit(1)

  with open(args.prov_img, 'rb') as f:
    provisioning_app = f.read()

  # Connect to the device
  logger.info("Opening SerialWire connection to the device")
  jlink_xml = os.path.join(os.path.dirname(__file__), "jlink/JLinkDevices.xml")
  sw = SerialWire(soc['device'], args.jlink_ser, args.jlink_host, jlink_xml)
  sw.connect()
  sw.reset_and_halt()
  logger.info("Connection opened")

  try:
    # Retrieve device MAC
    logger.info("Retrieving device serial number")
    sn = sw.get_mac_address()
    logger.info("Device serial number: %s", sn)

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

    # Generate Wi-SUN key-pair
    logger.info("Generating Wi-SUN key pair on the device")
    tx = wisun.command.GenerateKeyPair(0x100)
    sw.rtt_send(tx)
    rx = sw.rtt_receive()
    resp = wisun.response.GenerateKeyPair(rx)
    assert resp.status in (0, 19), f"Failure during Wi-SUN key pair generation ({resp.status})"
    if resp.status == 19:
      logger.warning("Wi-SUN key pair already exists")
    else:
      logger.info("Wi-SUN key pair generated")

    # Generate Wi-SUN CSR
    logger.info("Generating Wi-SUN CSR on the device")
    tx = wisun.command.GenerateCsr(0x100)
    sw.rtt_send(tx)
    rx = sw.rtt_receive()
    resp = wisun.response.GenerateCsr(rx)
    assert resp.status == 0, f"Failure during Wi-SUN CSR generation ({resp.status})"
    logger.info("Wi-SUN CSR generated")

    # Generate Wi-SUN device certificate
    logger.info("Generating Wi-SUN device certificate")
    device, batch, root = SigningServer.GetCerts(resp.csr, sn, args.config)
    logger.info("Wi-SUN device certificate generated")

    # Write Wi-SUN device certificate into NVM
    logger.info("Saving Wi-SUN device certificate into NVM")
    tx = ddp.command.WriteNvm(0x100, device)
    sw.rtt_send(tx)
    rx = sw.rtt_receive()
    rx = ddp.response.WriteNvm(rx)
    assert resp.status == 0, f"Failure saving Wi-SUN device certificate into NVM ({resp.status})"
    logger.info("Wi-SUN device certificate saved")

    # Write Wi-SUN batch certificate into NVM
    logger.info("Saving Wi-SUN batch certificate into NVM")
    tx = ddp.command.WriteNvm(0x101, batch)
    sw.rtt_send(tx)
    rx = sw.rtt_receive()
    rx = ddp.response.WriteNvm(rx)
    assert resp.status == 0, f"Failure saving Wi-SUN batch certificate into NVM ({resp.status})"
    logger.info("Wi-SUN batch certificate saved")

    # Write Wi-SUN root certificate into NVM
    logger.info("Saving Wi-SUN root certificate into NVM")
    tx = ddp.command.WriteNvm(0x102, root)
    sw.rtt_send(tx)
    rx = sw.rtt_receive()
    rx = ddp.response.WriteNvm(rx)
    assert resp.status == 0, f"Failure saving Wi-SUN root certificate into NVM ({resp.status})"
    logger.info("Wi-SUN root certificate saved")
  finally:
    sw.rtt_stop()
    sw.reset()
    sw.close()

  logger.info("Finished")
