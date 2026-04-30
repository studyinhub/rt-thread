#include <stdio.h>
#include <string.h>
#include <rtconfig.h>
#include <rtdevice.h>
#include "drv_sys.h"
#include <drv_gpio.h>
#include <fal.h>

#include "shell.h"
#include "msh.h"

#include "myWdt.h"
#include "myEth.h"

#include "board.h"

#include "myConfig.h"

#include "mySerial.h"
#include "myThreads.h"

int main() {
  rt_err_t ret;
  enable_wdt();
  load_config();

  myEth_init();
  init_ser_ports();

  threads_init();
  easyblink_stop(led_wrk);
  easyblink_stop(led_run);

  // easyblink(led_run, -1, 100, 500);

  while (1) {
    feed_wdt(1);

    rt_thread_mdelay(1000);
  }

  return 0;
}