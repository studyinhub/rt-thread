# NK-980IOT 测评之 WDT & CRYPTO

## WDT

看门狗定时器,在 RT-Thread 的文档里已经说过 https://www.rt-thread.org/document/site/#/rt-thread-version/rt-thread-standard/programming-manual/device/watchdog/watchdog，看门狗是一个特殊的定时器，本质还是定时器，这个定时器特殊之处在于，当系统运行到一个未知的状态时，就会自动复位系统，避免系统进入位置的无限循环状态，也就是俗称的‘死机'。

所以，这个定时器启动倒计时后，就必须在设定的时间内去主动去唤醒系统，俗称'喂狗',如果没有这个动作，计时到就会重启系统。

NUC980 的看门狗支持从空闲模式/或者 power-down 模式唤醒。

特点： 20-bit 的计数器

Selectable time-out interval (24 ~ 220) and the time-out interval is 0.48828125ms ~ 32s if
WDT_CLK = 32.768 kHz

这里更正一处 datasheet 错误,原文如上，24 ~220 看了半天没看明白，实际应该是 2^4 和 2^20.

可选择的超时间隔（2^4 ~ 2^20 ），如果 WDT_CLK 是 32.768 kHz，那么时间是 0.48823125ms - 32s 之间

1/**32765** * **2**^**4**  **=>** **0.0005**

1/**32765** * **2**^**20** **=>** **32.0029**

系统会保持在一个复位状态，这个时间很短，而且是可以选择的。

支持上电后强制开启看门狗，或者复位看门狗。

如果看门狗时钟源选择外部 LXT， 支持 WDT 超时 wake-up 功能。

WDT 的驱动比较简单就一个函数：WDT_Open

那么实际的时钟是多少呢？
msh />nu_clocks
SYS_UPLL = 300 MHz
SYS_APLL = 300 MHz
SYS_SYSTEM = 300 MHz
SYS_HCLK = 150 MHz
SYS_PCLK01 = 150 MHz
SYS_PCLK2 = 75 MHz
SYS_CPU = 300 MHz
CLK_HCLKEN = 41014D7F
CLK_PCLKEN0 = 00012001
CLK_PCLKEN1 = 00000031
AIC_INTMSK0 = 08281002
AIC_INTMSK1 = 00020014
AIC_INTEN0 = 00000000
AIC_INTEN1 = 00000000
AIC_INTDIS0 = 00000000
AIC_INTDIS1 = 00000000

默认情况下，看门狗的时钟被设置为 XT1_IN/512,也就是：

12000000 / **512** **=>** **23,437.5**

通过 wdt_get_working_hz（) 获得的值也是上面的值，如果要看门狗响应更快 ，就要给它更高的时钟。通俗讲,时钟频率越高，狗越机警。

在这个频率下，定时器超时间隔的时间范围是：

512/12000000 * 2^4 ~ 512/12000000 * 2^20

0.0007 ~ 44.7392

定时器一般都是有中断，周期地去中断，那么看门狗定时器的周期是多少呢？

/* Pick a suitable wdt timeout interval, it is a trade-off between the
   consideration of timeout accuracy and the system performance. The MIN_CYCLES
   parameter is a numerical value of the toutsel setting, and it must be set to
   a correct one which matches to the literal meaning of MIN_TOUTSEL. */
#define MIN_TOUTSEL                 (WDT_TIMEOUT_2POW10）

这个周期我们不能太快也不能太长，太快将频繁的进出系统中断，消耗系统性能，太慢又会响应不及时精度太低。

所以在 (4 ~ 20 ) 之间取一个值，这里取 10 作为一种权衡。

**512** /**12000000 * 2……10 **=>** **0.0437**

也就是，默认情况下，这个定时器会每 44ms 左右进一次中断。

也就是说，如果没有额外的机制，那么这条狗必须在 44ms 内喂一次，否则它就复位。

rtt 的看门狗移植在这个基础上增加了一个软件定时器，可以支持到更长的时间(如果没有这个，我们刚才算过默认频率下最多是 44s 左右).

实际上在 RTT 喂狗是重置的软件定时器计数，并不是直接去喂 WDT。

这个软件定时器的工作就是每当个 44 ms 左右的中断中去看一下软定时器的计数是否到了，如果没到，就帮忙喂一次狗，如果到了并且也喂软狗了，那么也喂一次 WDT。

如果没有喂狗，那么不好意思，就会重启系统。

所以，这个利用看门狗定时器中断构建的软件定时器，在执行较长的任务的时候，还是非常有用的，很有可能会超过 44s，这个时候我们可以设置一个更长的时间来避免系统重启。而且这个软定时器的超时时间，是可以在程序中动态修改的。

我们按照 rtt 的文档实际来看一下看门狗的运行吧,在官网的例子上做了一点修改，

通常喂狗的操作在 rt_thread_idle_sethook 中去执行，这里为了测试效果，改为在一个软定时器中去执行。

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

    /* 设置看门狗溢出时间*/
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

wdt_sample  wdt 3 1

[234462] I/myWdt: Watchdog is ON!Don't forget feed the dog in 3 seconds peroidly!
msh />[235470] D/myWdt: 喂狗1次
[236470] D/myWdt: 喂狗2次
[237470] D/myWdt: 喂狗3次
[240595] D/NO_TAG: 超时了

980 IBR 20180813
Boot from USB

可以看到一次喂狗到超时差不多就 3s 的时间，然后就重启了。

## CRYPTO

关于 CRYPTO 的基础，见

https://www.rt-thread.org/document/site/#/rt-thread-version/rt-thread-standard/programming-manual/device/crypto/crypto

在 NUC980 上 Crypto 称为 Cryptographic Accelerator 密码加速器。

包含一个安全的伪随机数发生器（PRNG）内核，支持 AES，SHA，HMAC，RSA 和 ECC 算法。

PRNG 支持 64bits,128bits,192bits 和 256bits 的随机数生成。

AES 加速器，是一个完整兼容 AES 标准的加解密算法。支持 ECP，CBC，CFB，OFB，CTR，CBC-CS1，CBC-CS2 和 CBC-CS3 模式。

SHA 加速器完全兼容 SHA-160，SHA-224，SHA-256，SHA-384 和 SHA-512 以及相应的 HMAC 算法。

ECC 椭圆曲线加密算法，是一种基于椭圆曲线数学的公开密钥加密算法。

其它具体的参考手册。

对称加密算法：AES

非对称加密算法：RSA，ECC（二代身份证和虚拟货币比特币区块链有采用)

哈希算法（摘要算法)：SHA，HMAS (更安全的基于哈希的消息摘要算法)

这里结合 RTT 官方示例的基础上做一些测试,首先非常感谢 rtt 做了大量的基础工作，能够避免我们在造轮子上去浪费宝贵的时间。

我创建了两个文件:myCrypto.c 和 myCrpto.h 两个文件，用来整合一下示例中的代码，结合 msh 做一些测试。

msh />crypto_sample
[6982] D/HEX Data   : 0000-0007: 00 01 02 03 04 05 06 07    ........
                      0008-000F: 08 09 0A 0B 0C 0D 0E 0F    ........
                      0010-0017: 10 11 12 13 14 15 16 17    ........
                      0018-001F: 18 19 1A 1B 1C 1D 1E 1F    ........
[7005] D/HEX AES-enc: 0000-0007: 0A 94 0B B5 41 6E F0 45    ....An.E
                      0008-000F: F1 C3 94 58 C6 53 EA 5A    ...X.S.Z
                      0010-0017: 3C F4 56 B4 CA 48 8A A3    <.V..H..
                      0018-001F: 83 C7 9C 98 B3 47 97 CB    .....G..
[7030] D/HEX AES-dec: 0000-0007: 00 01 02 03 04 05 06 07    ........
                      0008-000F: 08 09 0A 0B 0C 0D 0E 0F    ........
                      0010-0017: 10 11 12 13 14 15 16 17    ........
                      0018-001F: 18 19 1A 1B 1C 1D 1E 1F    ........
[120417] D/NO_TAG: 超时了
980 IBR 20180813
Boot from USB

实际测试 AES 的加解密是没有问题，但是 sha1 的测试没有成功，我的看门狗设置超时时间都 60s 了，依然还是超时了，在 7s 左右的时候就开始了，最终在 120s 超时，说明中间喂过一次狗，但狗还是饿死了。具体原因还不清楚，大家有谁知道原因吗？

static int crypto_sample(int argc, char *argv[])
{
    rt_err_t ret = RT_EOK;
    rt_uint8_t buf_in[32];
    rt_uint8_t buf_out[32];
    int i;

    /* 填充测试数据*/
    for (i = 0; i < sizeof(buf_in); i++)
    {
        buf_in[i] = (rt_uint8_t)i;
    }
    /* 打印填充的数据 */
    LOG_HEX("Data   ", 8, buf_in, sizeof(buf_in));

    memset(buf_out, 0, sizeof(buf_out));
    /* 对测试数据进行加密 */
    hw_aes_cbc(buf_in, buf_out, HWCRYPTO_MODE_ENCRYPT);

    /* 打印加密后的数据 */
    LOG_HEX("AES-enc", 8, buf_out, sizeof(buf_out));

    memset(buf_in, 0, sizeof(buf_in));
    /* 对加密数据进行解密 */
    hw_aes_cbc(buf_out, buf_in, HWCRYPTO_MODE_DECRYPT);

    /* 打印解密后的数据 */
    LOG_HEX("AES-dec", 8, buf_in, sizeof(buf_in));

    // memset(buf_out, 0, sizeof(buf_out));
    /* 对测试数据进行 MD5 运算 */
    // hw_hash(buf_in, buf_out, HWCRYPTO_TYPE_MD5);

    /* 打印 16 字节长度的 MD5 结果 */
    // LOG_HEX("MD5    ", 8, buf_out, 16);

    memset(buf_out, 0, sizeof(buf_out));
    /* 对测试数据进行 SHA1 运算 */
    hw_hash(buf_in, buf_out, HWCRYPTO_TYPE_SHA1);

    /* 打印 20 字节长度的 SHA1 结果 */
    LOG_HEX("SHA1   ", 8, buf_out, 20);

    return ret;
}
