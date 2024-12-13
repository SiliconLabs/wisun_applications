#!/usr/bin/env python3
# vim: set sw=2 expandtab:

# SoC configuration for Wi-SUN
socs = {
  'xg12': {'nvm3inststartaddress': 0x000f7000, 'nvm3instsize': 0x9000, 'device': 'EFR32MG12PxxxF1024', 'ramstartaddress': 0x20000000, 'stacksize': 0x1000},
  'xg25': {'nvm3inststartaddress': 0x081d4000, 'nvm3instsize': 0xA000, 'device': 'EFR32FG25BxxxF1920', 'ramstartaddress': 0x20000000, 'stacksize': 0x1000},
  'xg28': {'nvm3inststartaddress': 0x080f4000, 'nvm3instsize': 0xA000, 'device': 'EFR32FG28AxxxF1024', 'ramstartaddress': 0x20000000, 'stacksize': 0x1000},
}
