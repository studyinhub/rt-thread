#include <string.h>
#include <rtconfig.h>
#include <rtdevice.h>
#include "drv_sys.h"
#include <drv_gpio.h>
#include <fal.h>

#include <netdev_ipaddr.h>
#include <netdev.h>

#include "dfs_file.h"
#include "cJSON.h"
#include "main.h"
#include "myWdt.h"

#include "myConfig.h"
#include "modbus_x.h"
#include "myThreads.h"

#include "myWebnet.h"


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "main"
#define LOG_LVL LOG_LVL_INFO
#include <ulog.h>

static int set_date_time()
{
    int year, month, day, hour, min, sec;

    LOG_I("Init Time with build Time");
    // 先将实际设置为编译时间，然后通过 NTP 同步
    // char *token = strtok(BUILDTIME, ".");
    // year = atoi(token);

    // token = strtok(NULL, ".");
    // month = atoi(token);

    // token = strtok(NULL, ".");
    // day = atoi(token);

    // token = strtok(BUILDTIME, ":");
    // hour = atoi(token);

    // token = strtok(NULL, ":");
    // min = atoi(token);

    // token = strtok(NULL, ":");
    // sec = atoi(token);

    if (sscanf(BUILDTIME, "%d.%d.%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6)
    {
        // LOG_D("year:%d,month:%d,day:%d", year, month, day);
        // LOG_D("hour:%d,min:%d,sec:%d", hour, min, sec);
    }
    else
    {
        LOG_E("sscanf error");
    }

    set_time(hour, min, sec);
    set_date(year, month, day);
    print_time();
}

// INIT_PREV_EXPORT(set_date_time);

#include <netdev.h> /* 包含全部的 netdev 相关操作接口函数 */

struct netdev *netdev_eth = RT_NULL;

void netdev_callback_eth(struct netdev *netdev, enum netdev_cb_type type)
{
    switch (type)
    {
    case NETDEV_CB_STATUS_LINK_UP:
        LOG_I("Ethernet LINK UP");
        // init_tftps(WEB_ROOT);
        // init_webnet(WEB_ROOT);
        // system("dns e0 0 8.8.8.8");
        // system("dns e0 1 114.114.114.114");
        break;
    case NETDEV_CB_STATUS_LINK_DOWN:
        LOG_W("Ethernet LINK DOWN");
        break;
    default:
        break;
    }
}

static struct rt_timer timer1;
static void cb_timer1(void *parameter) { log_d("Timer:%s", parameter); }


int main(int argc, char **argv)
{
    rt_err_t ret;
    // LOG_I("The current version of APP firmware is %s\n", versionString);
    sprintf(buildtime, "%s", BUILDTIME);
    // fal_init();
    printVersion();
    set_date_time();
    enable_wdt();
    load_config();

    init_ser_ports();

    log_d("web_root:%s", WEB_ROOT);

    rt_timer_init(&timer1, "tm1", cb_timer1, "tm1", RT_TICK_PER_SECOND,
                RT_TIMER_FLAG_PERIODIC);
    rt_timer_start(&timer1);

    threads_init();

    rt_thread_t tid_m_poll = RT_NULL, tid2 = RT_NULL, tid_s_poll = RT_NULL;

    rt_uint32_t level;


    do
    {
        netdev_eth = netdev_get_by_name("e0");
        if (netdev_eth)
        {
            netdev_set_status_callback(netdev_eth, netdev_callback_eth);
            break;
        }
        else
        {
            LOG_W("GET eth netdev failed,retry!");
            rt_thread_mdelay(1000);
        }
    } while (netdev_eth == RT_NULL);

    while (1)
    {
        feed_wdt(1);


        easyblink(led_wrk, -1, 1000, 1000);
        easyblink(led_run, -1, 100, 500);
        rt_thread_mdelay(1000);
    }
__exit:
    return 0;
}

static rt_timer_t oneshort_timer; // on-short
static rt_bool_t oneshort_timer_flag = RT_FALSE;

static void oneshort_timer_cb(void *parameter)
{

    LOG_D("one shot timer is timeout");
    oneshort_timer_flag = RT_TRUE;
}
