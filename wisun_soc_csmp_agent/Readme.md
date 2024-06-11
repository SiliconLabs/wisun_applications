<table border="0">
  <tr>
    <td align="left" valign="middle">
    <h1>Wi-SUN - SoC CSMP Agent</h1>
  </td>
  <td align="left" valign="middle">
    <a href="https://www.silabs.com/wireless/wi-sun">
      <img src="http://pages.silabs.com/rs/634-SLU-379/images/WGX-transparent.png"  title="Silicon Labs Gecko and Wireless Gecko MCUs" alt="EFM32 32-bit Microcontrollers" width="100"/>
    </a>
  </td>
  </tr>
</table>


# Summary

This project is a reference implementation of the CSMP agent hosted on [csmp-agent-lib](https://github.com/CiscoDevNet/csmp-agent-lib) to allow a node to connect to FND.

# Requirements

## Hardware Prerequisites

One of the supported platforms listed below is required to run the example:

- [EFR32FG25 Sub-GHz Wireless SoCs](https://www.silabs.com/wireless/proprietary/efr32fg25-sub-ghz-wireless-socs)
- [EFR32FG28 Sub-GHz Wireless + 2.4 GHz BLE SoCs](https://www.silabs.com/wireless/proprietary/efr32fg28-sub-ghz-wireless-socs)

## Software Prerequisites

- Simplicity Studio v5 and the Simplicity SDK Suit 2024.6.0.

## Install Simplicity Studio 5 and the Gecko SDK

Simplicity Studio 5 is a free software suite needed to start developing your application. To install Simplicity Studio 5, please follow this [**procedure**](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-getting-started/install-ss-5-and-software).

To install the Simplicity SDK Suit follow this [**procedure**](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-getting-started/install-ss-5-and-software#install-software) by selecting the options [**Install by Technology Type**](https://docs.silabs.com/simplicity-studio-5-users-guide/latest/ss-5-users-guide-getting-started/install-ss-5-and-software#install-software-by-technology-type).

## Import the Project to Simplicity Studio 5

If this step is not done already, follow the steps listed under [Add the Wi-SUN applications repository to Simplicity Studio 5](../README.md#add-the-wi-sun-applications-repository-to-simplicity-studio-5) section to create the project using Simplicity Studio.

# Start the Example

1. Create the project by following the steps listed under [Create the Wi-SUN applications example projects](../README.md#create-the-wi-sun-applications-example-projects) section.
2. Build the project and flash it on your board.
3. Refer to the folder **Vendors/Silabs** on the [CSMP Agent library](https://github.com/CiscoDevNet/csmp-agent-lib) to complete your setup and connect to FND.

> [!IMPORTANT]  
>At the time of writing this guide, the CSMP Agent port to FreeRTOS pull request is still open and not yet approved. If you can't find the folder **Vendors/Silabs** on the main branch please check the open pull requests.

