#include "sl_system_init.h"
#include "sl_system_kernel.h"

int main(void)
{
  // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
  // Note that if the kernel is present, processing task(s) will be created by
  // this call.
  sl_system_init();

  // Start the kernel. Task(s) created in app_init() will start running.
  sl_system_kernel_start();
  while (1) ;
}
