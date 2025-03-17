#ifndef __MY_WDT_H__
#define __MY_WDT_H__
#include <rtdevice.h>
#define RT_DEVICE_CTRL_WDT_GET_TIMEOUT (1)  /* 获取溢出时间 */
#define RT_DEVICE_CTRL_WDT_SET_TIMEOUT (2)  /* 设置溢出时间 */
#define RT_DEVICE_CTRL_WDT_GET_TIMELEFT (3) /* 获取剩余时间 */
#define RT_DEVICE_CTRL_WDT_KEEPALIVE (4)    /* 喂狗 */
#define RT_DEVICE_CTRL_WDT_START (5)        /* 启动看门狗 */
#define RT_DEVICE_CTRL_WDT_STOP (6)         /* 停止看门狗 */

#define WDT_DEVICE_NAME "wdt" /* 看门狗设备名称 */
#define WDT_TIMEOUT 60 // 10s 内必须喂狗

extern rt_device_t wdg_dev; /* 看门狗设备句柄 */
extern int enable_wdt();
#endif
