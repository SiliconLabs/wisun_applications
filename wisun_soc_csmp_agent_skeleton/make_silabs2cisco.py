#!/usr/bin/env python3

import sys
import os
import re
from pathlib import Path


if __name__ == '__main__':
    abspath = os.path.abspath(__file__)
    dname = os.path.dirname(abspath)
    os.chdir(dname)

    destination_file = r"efr32_wisun.mak"
    
    # Find and validate the silabs makefile
    p = Path(r'./').glob('**/*')
    files = [x for x in p if x.is_file()]
    
    original_makefile = None

    for file in files:
        if re.match(r"[\w\d_\-]+.project\.mak$", file.name):
            original_makefile = file
            print(f"* Original silabs makefile: {file}")
            break
    
    # Find GSDK directory
    sisdk_dir = None
    p = Path(r'./').glob('*')
    dirs = [x for x in p if x.is_dir()]
    for d in dirs:
        if re.match(r"simplicity_sdk_[\d]+.[\d]+.[\d]+$", d.name):
            sisdk_dir = d
            print(f"* Silabs SDK directory: {d}")
            break
    
    
    if not original_makefile:
        print("No original makefile found")
        sys.exit(1)
        
    if not sisdk_dir:
        print("No silabs SDK directory found")
        sys.exit(1)
    
    lines = []
    with open(original_makefile, 'r') as f:
        lines = f.readlines()
        
    with open(destination_file, 'w') as f:
        f.write(f"CSMP_AGENT_LIB_EFR32_WISUN_PATH ?= Vendors/Silabs\n")
        for line in lines:
            if re.match(r"\s*#", line):
              continue
            
            # Change Copied SDK PATH  
            if re.match(r"^COPIED_SDK_PATH", line):
              line = f"COPIED_SDK_PATH ?= $(CSMP_AGENT_LIB_EFR32_WISUN_PATH)/{sisdk_dir.name}\n"
            
            # Remove main
            if re.match(r"CDEPS\s*\+=\s*\$\(OUTPUT_DIR\)/project/main\.d", line):
              continue
            if re.match(r"OBJS\s*\+=\s*\$\(OUTPUT_DIR\)/project/main\.o", line):
              continue
            
            #remove pre and post build rules
            if re.match(r"^pre-build\:", line):
              continue
            
            if re.match(r"^post-build\:", line):
              continue
            
            # Add CSMP_AGENT_LIB_EFR32_WISUN_PATH to autogen path
            m = re.search(r'([\s+"]+)autogen', line)
            if m:
              line = line.replace(f"{m.group(1)}autogen", f"{m.group(1)}$(CSMP_AGENT_LIB_EFR32_WISUN_PATH)/autogen")
              f.write(line)
              continue
            
            # Add CSMP_AGENT_LIB_EFR32_WISUN_PATH to component path
            m = re.search(r'([\s+"]+)component', line)
            if m:
              line = line.replace(f"{m.group(1)}component", f"{m.group(1)}$(CSMP_AGENT_LIB_EFR32_WISUN_PATH)/component")
              f.write(line)
              continue
            
            # Update include path with CSMP_AGENT_LIB_EFR32_WISUN_PATH
            m = re.search(r'(\-I\s*)autogen', line)
            if m:
              line = line.replace(f"{m.group(1)}autogen", f"{m.group(1)}$(CSMP_AGENT_LIB_EFR32_WISUN_PATH)/autogen")
              f.write(line)
              continue
            
            m = re.search(r'(\-I\s*)config', line)
            if m:
              line = line.replace(f"{m.group(1)}config", f"{m.group(1)}$(CSMP_AGENT_LIB_EFR32_WISUN_PATH)/config")
              f.write(line)
              continue
            
            m = re.search(r'(\-I\s*)component', line)
            if m:
              line = line.replace(f"{m.group(1)}component", f"{m.group(1)}$(CSMP_AGENT_LIB_EFR32_WISUN_PATH)/component")
              f.write(line)
              continue
            
            # Handle any .c source file in the project route
            m = re.search(r'\s+([\d\w_-]+\.c)', line)
            if m:
              line = line.replace(f"{m.group(1)}", f"$(CSMP_AGENT_LIB_EFR32_WISUN_PATH)/{m.group(1)}")
              f.write(line)
              continue
            
            
            f.write(line)
        
    print(f"Silabs - Cisco CsmpAgent make file created successfully:\n    {destination_file}")
    