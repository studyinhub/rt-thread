#include "board.h"
#include "myWdt.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "myWdt"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>


rt_device_t wdg_dev; /* 看门狗设备句柄 */

static void idle_hook(void)
{
    /* 在空闲线程的回调函数里喂狗 */

    if (wdg_dev != RT_NULL)
    {
        // rt_pin_write(LED_1, !(rt_pin_read(LED_1)));
        rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, NULL);
    }
}
// #define ENABLE_WDT RT_TRUE
int enable_wdt()
{
    rt_err_t ret;
    rt_uint32_t timeout = WDT_TIMEOUT;
    wdg_dev = rt_device_find("wdt");
    
    // if(!ENABLE_WDT) return RT_ERROR;

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

    rt_thread_idle_sethook(idle_hook);
}
INIT_DEVICE_EXPORT(enable_wdt);



// #define WDT_TEST

#ifdef WDT_TEST

static struct rt_timer timer1;
static int cnt = 0;

static void cb_timer1(void *parameter)
{
    if (cnt++ >= 2)
    {
        rt_timer_stop(&timer1);
    }
    
    log_d("喂狗%d次",cnt);
    rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_KEEPALIVE, NULL);
}


static int wdt_sample(int argc, char *argv[])
{
    rt_err_t ret = RT_EOK;
    rt_uint32_t timeout = 10;        /* 溢出时间，单位：秒 */
    rt_uint32_t timer1_timeout_seconds = RT_TICK_PER_SECOND;
    char device_name[RT_NAME_MAX];

    /* 判断命令行参数是否给定了设备名称 */
    if (argc >= 2)
    {
        rt_strncpy(device_name, argv[1], RT_NAME_MAX);
    }
    else
    {
        rt_strncpy(device_name, WDT_DEVICE_NAME, RT_NAME_MAX);
    }

    if(argc >=3)
    {
        sscanf(argv[2],"%d",&timeout);
        log_d("timeout:%d",timeout);
    }

    if(argc >=4)
    {
        sscanf(argv[3],"%d",&timer1_timeout_seconds);
        log_d("timer1_timeout_seconds:%d",timer1_timeout_seconds);
        timer1_timeout_seconds = timer1_timeout_seconds * RT_TICK_PER_SECOND;
    }


    /* 根据设备名称查找看门狗设备，获取设备句柄 */
    wdg_dev = rt_device_find(device_name);
    if (!wdg_dev)
    {
        rt_kprintf("find %s failed!\n", device_name);
        return RT_ERROR;
    }

    /* 设置看门狗溢出时间 */
    ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_SET_TIMEOUT, &timeout);
    if (ret != RT_EOK)
    {
        rt_kprintf("set %s timeout failed!\n", device_name);
        return RT_ERROR;
    }
    /* 启动看门狗 */
    ret = rt_device_control(wdg_dev, RT_DEVICE_CTRL_WDT_START, RT_NULL);
    if (ret != RT_EOK)
    {
        rt_kprintf("start %s failed!\n", device_name);
        return -RT_ERROR;
    }
    log_i("Watchdog is ON!Don't forget feed the dog in %d seconds peroidly!", timeout);
    
    /* 设置空闲线程回调函数 */
    // rt_thread_idle_sethook(idle_hook);

    rt_timer_init(&timer1,"tm1",cb_timer1,RT_NULL,timer1_timeout_seconds,RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(&timer1);

    return ret;
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(wdt_sample, wdt sample);


#endif
