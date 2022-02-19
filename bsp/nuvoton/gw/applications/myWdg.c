#include "myWdg.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "myWdg"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

#define WDT_DEVICE_NAME "wdt" /* 看门狗设备名称 */
rt_device_t wdg_dev; /* 看门狗设备句柄 */

#define ENABLE_WDT RT_TRUE
int enable_wdt()
{
    rt_err_t ret;
    rt_uint32_t timeout = 26; //26
    if (ENABLE_WDT)
    {
        wdg_dev = rt_device_find("wdt");

        if (!wdg_dev)
        {
            rt_kprintf("find %s failed!\n", "wdt");
            return RT_ERROR;
        }

        ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout);
        if (ret != RT_EOK)
        {
            rt_kprintf("set %s timeout failed!\n", "wdt");
            return RT_ERROR;
        }

        ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL);
        if (ret != RT_EOK)
        {
            rt_kprintf("start %s failed!\n", "wdt");
            return -RT_ERROR;
        }
        log_i("Watchdog is ON!Don't forget feed the dog in %d seconds peroidly!", timeout);
    }
}
INIT_DEVICE_EXPORT(enable_wdt);