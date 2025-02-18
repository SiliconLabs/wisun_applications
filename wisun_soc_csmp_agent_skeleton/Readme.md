<table style="border: none; border-collapse: collapse;" align="center">
  <tr>
    <td align="center" valign="middle" style="border: none;">
      <h1>Wi-SUN - SoC CSMP Agent Skeleton</h1>
      <a href="https://www.silabs.com/wireless/wi-sun">
        <img src="https://silabs.scene7.com/is/image/siliconlabs/wi-sun-color?$TransparentPNG$"  title="Wi-SUN" alt="Wi-SUN Logo" width="250" />
      </a>
    </td>
    <td align="center" valign="middle" style="border: none;">
      <a href="https://www.silabs.com/wireless/wi-sun">
        <img src="http://pages.silabs.com/rs/634-SLU-379/images/WGX-transparent.png"  title="Silicon Labs Gecko and Wireless Gecko MCUs" alt="EFR32 32-bit Wireless Microcontrollers" width="200"/>
      </a>
    </td>
  </tr>
</table>


# Summary

This readme covers the steps needed to get started using the project the **Wi-SUN - SoC CSMP Agent Skeleton** that can be built using the [csmp-agent-lib](https://github.com/CiscoDevNet/csmp-agent-lib) build system to allow a Wi-SUN node to connect to FND.

# Requirements

## Hardware Prerequisites

One of the supported platforms listed below is required to run the example:

- [EFR32FG25 Sub-GHz Wireless SoCs](https://www.silabs.com/wireless/proprietary/efr32fg25-sub-ghz-wireless-socs)
- [EFR32FG28 Sub-GHz Wireless + 2.4 GHz BLE SoCs](https://www.silabs.com/wireless/proprietary/efr32fg28-sub-ghz-wireless-socs)

## Software Prerequisites

- Simplicity Studio v5 with the Simplicity SDK Suit 2024.6.0.

## Install Simplicity Studio 5 and the Gecko SDK

Simplicity Studio 5 is a free software suite needed to start developing your application. To install Simplicity Studio 5, please follow this [**procedure**](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-getting-started/install-ss-5-and-software).

To install the Simplicity SDK Suit follow this [**procedure**](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-getting-started/install-ss-5-and-software#install-software) by selecting the options [**Install by Technology Type**](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-getting-started/install-ss-5-and-software#install-software-by-technology-type).

## Import the Project to Simplicity Studio 5

If this step is not done already, follow the steps listed under [Add the Wi-SUN applications repository to Simplicity Studio 5](../README.md#add-the-wi-sun-applications-repository-to-simplicity-studio-5) section to add the projects to Simplicity Studio.

# Project creation

1. Create the Bootloader project and the Skeleton Application by following the steps listed under [Create the Wi-SUN applications example projects](../README.md#create-the-wi-sun-applications-example-projects) section. 
2. Generate the "Bootloader - SoC SPI Flash Storage (single image with slot size of 1024kB)"" project from Simplicity Studio and configure the storage slots in config/btl_storage_slot_cfg.h:
    ```
    // <e SLOT0_ENABLE> Enable Slot 0
    // <i> Default: 1
    #define SLOT0_ENABLE  1

    // <o SLOT0_START> Start Address
    // <f .h>
    #define SLOT0_START   0

    // <o SLOT0_SIZE> Slot Size
    // <f .h>
    #define SLOT0_SIZE 524288

    // <e SLOT1_ENABLE> Enable Slot 1
    // <i> Default: 0
    #define SLOT1_ENABLE  1

    // <o SLOT1_START> Start Address
    // <f .h>
    #define SLOT1_START 524288

    // <o SLOT1_SIZE> Slot Size
    // <f .h>
    #define SLOT1_SIZE 524288
    ```
3. Add "GBL Compression (LZMA)" component to the bootloader projcet in Simplicity Studio to be able to decompress the GBL files. 
4. Build and deploy the Bootloader project on the target SoC by using Simplicity Commander.
5. In the Skeleton Application **project configuration** perspective, select the **Copy Contents** option and click **Finish**. This step is very important to build the project successfully. 
6. Generate the project makefiles for the Skeleton Application in Simplicity Studio by following these steps:
    * Go to the **OVERVIEW** tab in the slcp file perspective.
    * Scroll down to the end of the *Target and Tool Settings* card and click **Change Target/SDK/Generators**.
    * In the *CHANGE PROJECT GENERATORS* list, select **GCC Makefile**.
    * Click Save and wait for Simplicity Studio to generate the project.
7. Refer to the folder **Vendors/Silabs** on the [CSMP Agent library](https://github.com/CiscoDevNet/csmp-agent-lib) to complete your setup and connect to FND.


> [!IMPORTANT]  
>At the time of writing this guide, the Silicon Labs EFR32 Wi-SUN platforms support pull request on the [CSMP Agent library](https://github.com/CiscoDevNet/csmp-agent-lib) is still open and not yet approved. If you can't find the folder **Vendors/Silabs** on the main branch please check the open pull requests.

