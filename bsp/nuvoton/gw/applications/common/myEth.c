#include "myEth.h"

#include <netdev.h> /* 包含全部的 netdev 相关操作接口函数 */

#include "myWebnet.h"

#define LOG_TAG "MyEth"
#define LOG_LVL LOG_LVL_INFO
#include <ulog.h>

struct netdev *netdev_eth = RT_NULL;

void netdev_callback_eth(struct netdev *netdev, enum netdev_cb_type type) {
  switch (type) {
  case NETDEV_CB_STATUS_LINK_UP:
    easyblink(led_wrk, -1, 800, 1000);
    // log_d("Ethernet LINK UP webroot:%s", WEB_ROOT);
    init_tftps(WEB_ROOT);
    init_webnet(WEB_ROOT);
    system("dns e0 0 8.8.8.8");
    system("dns e0 1 114.114.114.114");
    break;
  case NETDEV_CB_STATUS_LINK_DOWN:
    easyblink_stop(led_wrk);
    // log_w("Ethernet LINK DOWN");
    break;
  default:
    break;
  }
}

int myEth_init() {
  do {
    netdev_eth = netdev_get_by_name("e0");
    if (netdev_eth) {
      netdev_set_status_callback(netdev_eth, netdev_callback_eth);
      break;
    } else {
      LOG_W("GET eth netdev failed,retry!");
      rt_thread_mdelay(1000);
    }
  } while (netdev_eth == RT_NULL);

  return 0;
}
