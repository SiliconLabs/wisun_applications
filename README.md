<table border="0">
  <tr>
    <td align="left" valign="middle">
      <h1>EFR32 Wi-SUN Application Examples</h1>
      <a href="https://www.silabs.com/wireless/wi-sun">
        <img src="images/../wisun_node_monitoring/image/wi-sun-logo.jpg"  title="Wi-SUN" alt="Wi-SUN Logo" width="250" />
      </a>
    </td>
    <td align="left" valign="middle">
      <a href="https://www.silabs.com/wireless/wi-sun">
        <img src="http://pages.silabs.com/rs/634-SLU-379/images/WGX-transparent.png"  title="Silicon Labs Gecko and Wireless Gecko MCUs" alt="EFR32 32-bit Wireless Microcontrollers" width="250"/>
      </a>
    </td>
  </tr>
</table>

# Silicon Labs Wi-SUN Applications #

The Silicon Labs Wi-SUN stack allows for a wide variety applications to be built on its foundation. This repo showcases some example applications built using the Silicon Labs Wi-SUN stack.

## Examples ##

- [Wi-SUN Node Monitoring](https://github.com/SiliconLabs/wisun_applications_staging/tree/main/wisun_node_monitoring): An extendable node monitoring application providing information on the device/board/application and connected statistics. It can be easily extended to monitor sensors and control actuators.
- [Wi-SUN SoC CSMP Agent Skeleton](/wisun_soc_csmp_agent/): A reference implementation of the CSMP agent sample application skeleton that can be built using the [csmp-agent-lib](https://github.com/CiscoDevNet/csmp-agent-lib) build system, to allow a node to connect to FND.

## Add the 'Wi-SUN Applications' Repository to Simplicity Studio 5 ##

1. Download and install [Simplicity Studio 5](https://www.silabs.com/developers/simplicity-studio).
2. On Simplicity Studio 5, go to **Window -> Preferences -> Simplicity Studio -> External Repos**.
3. Click **[Add]**. In the **URI** field, copy & paste the following link: `https://github.com/SiliconLabs/wisun_applications.git`
4. Click **[Next]** then **[Finish]** and **[Apply and Close]**.

## Create the Wi-SUN Applications Example Projects ##

1. Connect the Silicon Labs Starter Kit and open Simplicity Studio 5.
2. Select the **[Launcher]** perspective.
3. From the **[Debug Adapters]** panel on the left top corner, select your Silicon Labs Starter Kit.
4. Ensure that an SDK is selected in the **[General Information]** tile of the **[Overview]** tab.
5. Select the **[EXAMPLE PROJECTS & DEMOS]** tab in **[Launcher]** perspective.
6. Under **Provider** (at the bottom of the list), select **wisun_applications** and click **[create]** on the desired project.
   1. If the project you are looking for is not listed in **wisun_applications**, it is possible that your debug adapter is not listed as compatible with the application. Try using a Silicon Labs debug adapter first, and check if it is listed in the project's **boardCompatibility** section of [templates.xml](templates.xml).

## Using Wi-SUN Crash Handler in Another Project ##

Wi-SUN Node Monitoring application contains a crash handler component that reports and recovers from application asserts and crashes.

In order to utilize the component in another project:

1. Copy **sl_wisun_crash_handler.c** and **sl_wisun_crash_handler.h** to the project, and add them to compilation.
2. Add RMU (emlib_rmu) component to the project.
3. Add linker flags below to the project. In Studio, go to **Project -> Properties -> C/C++ Build -> Settings -> GNU ARM C Linker -> Miscellaneous**. The component will work without them, but it will be unable to capture some asserts on GCC.

```text
    -Wl,--wrap=__stack_chk_fail,--wrap=__assert_func
```

4. Call crash handler initialization in **app_init**.

```C
  #include "sl_wisun_crash_handler.h"
  ...
  void app_init(void)
  {
    sl_wisun_crash_handler_init();
  }
```

5. After initialization, read crash reason.

```C
  #include "sl_wisun_crash_handler.h"
  ...
  const sl_wisun_crash_t *crash;
  ...
  crash = sl_wisun_crash_handler_read();
  if (crash) {
    switch (crash->type) {
      case SL_WISUN_CRASH_TYPE_XXX:
        break;
      ...
    }
    sl_wisun_crash_handler_clear();
  }
```

6. Alternatively, call `sl_wisun_check_previous_crash()` to fill the `crash_info_string` then print the string. This string will be available during the entire application runtime (the Wi-SUN Node Monitoring application makes it available via a CoAP request).

```C
  #include "sl_wisun_crash_handler.h"
  ...
  int crash_type;
  ...
  crash_type = sl_wisun_check_previous_crash();
  if (crash_type) {
    printf("%s\n", crash_info_string);
  }
```

> [!WARNING]
> If the project uses Gecko Bootloader, bootloader's bss section may overlap with application's noinit section, causing the crash handler to not function properly. In Wi-SUN Node Monitoring application this is avoided by increasing
> the stack size **SL_STACK_SIZE**, which pushes application's noinit section further into RAM. Another possibility is to modify the generated linker script and swap the placement of application's bss and noinit sections.

## Documentation ##

- Official Wi-SUN documentation can be found in [the Wi-SUN pages on docs.silabs.com](https://docs.silabs.com/wisun/latest/wisun-start/).
- Project-specific information is provided in the **README.md** file in each project folder.

## Reporting Bugs/Issues and Posting Questions and Comments ##

To report bugs in the Wi-SUN Application Examples projects, please create a new "Issue" in the "Issues" section of this repo. Please reference the board, project, and source files associated with the bug, and reference line numbers. If you are proposing a fix, also include information on the proposed fix. Since these examples are provided as-is, there is no guarantee that these examples will be updated to fix these issues.

Questions and comments related to these examples should be made by creating a new "Issue" in the "Issues" section of this repo.

## Disclaimer ##

The Gecko SDK suite supports development with Silicon Labs IoT SoC and module devices. Unless otherwise specified in the specific directory, all examples are considered to be EXPERIMENTAL QUALITY which implies that the code provided in the repos has not been formally tested and is provided as-is.  It is not suitable for production environments.  In addition, this code will not be maintained and there may be no bug maintenance planned for these resources. Silicon Labs may update projects from time to time.
