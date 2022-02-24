/**************************************************************************/ /**
*
* @copyright (C) 2021 yangxiyuan. All rights reserved.
*
* Change Logs:
* Date            Author            Notes
* 2021-11-25      yangxiyuan        First version
*
******************************************************************************/
#include <string.h>
#include <rtconfig.h>
#include <rtdevice.h>
#include <drv_uart.h>

#define LOG_TAG "board"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>
#include "main.h"
#include "mnt.h"
#include "cJSON.h"
#include "config.h"



#if defined(BOARD_USING_STORAGE_SPINAND) && defined(NU_PKG_USING_SPINAND)

#include "drv_qspi.h"
#include "spinand.h"

// 一个 block 是 64 page，page 是 2k, block:128k
// uboot:0x100000 8 - 15 block ;1MB; 325k ~ 2.53 block, 325- 128*2 = 69k,  34.5page mtd_nand read nand0 10 43
// app:  0x200000 16-23 block; ;1MB;
// fs:   0x300000 24
// 24 个 block 是 3072k  3MB
// nand0 3MB 基本上存储的都是系统的 spl uboot app env，这些，所以 nand0 不要存入用户数据，这部分占用的是 3MB 之前的数据，给 app 留了 1MB 的空间
// nand1 的空间是 128-3 = 125MB， 起始地址是 0x300000 ,如果在这个位置烧写用户数据会怎么样，那么用到的文件系统是 uffs，如何生成 uffs 的 image 呢？

// nand2 是整个分区，128MB，
struct rt_mtd_nand_device mtd_partitions[MTD_SPINAND_PARTITION_NUM] =
    {
        [0] =
            {
                .block_start = 0,
                .block_end = 23,
                .block_total = 24,
            },
        [1] =
            {
                .block_start = 24,
                .block_end = 1023,
                .block_total = 1000,
            },
        [2] =
            {
                .block_start = 0,
                .block_end = 1023,
                .block_total = 1024,
            }};

static int rt_hw_spinand_init(void)
{
    if (nu_qspi_bus_attach_device("qspi0", "qspi01", 4, RT_NULL, RT_NULL) != RT_EOK)
        return -1;

    if (rt_hw_mtd_spinand_register("qspi01") != RT_EOK)
        return -1;

    return 0;
}

INIT_COMPONENT_EXPORT(rt_hw_spinand_init);
#endif //defined(BOARD_USING_STORAGE_SPINAND) && defined(NU_PKG_USING_SPINAND)

#include <drv_gpio.h>
#include "easyblink.h"

// 左 1 PA7                         右 1  PA11 UART8_RX
// 左 2 PA8                         右 2  PA12 UART8_TX
// 左 3 4G 信号灯，所以不受 CPU 控制   右 3  PA4  UART6 RX
// 左 4 电源灯                       右 4  PA5  UART6 TX

// 4G 模块 PC1 PC2 UART7

#define LED_1 NU_GET_PININDEX(NU_PA, 7) // 左 1
#define LED_2 NU_GET_PININDEX(NU_PA, 8) // 左 2
#define KEY_1 NU_GET_PININDEX(NU_PB, 7) // 16+7 23,低电平复位

ebled_t led_1 = RT_NULL;
ebled_t led_2 = RT_NULL;

int test_led(int argc, char **argv)
{
    rt_kprintf("argc:%d\n", argc);

    if (argc < 1)
    {
        rt_kprintf("请输入参数");
    }
    for (int i = 0; i < argc; i++)
    {
        rt_kprintf("%d:%s\n", i, argv[i]);
    }
    int pinNum = atoi(argv[1]);
    rt_pin_mode(pinNum, PIN_MODE_OUTPUT);

    if (!strcmp(argv[2], "lo"))
    {
        rt_pin_write(pinNum, PIN_LOW);
    }
    else
    {
        rt_pin_write(pinNum, PIN_HIGH);
    }
}

MSH_CMD_EXPORT(test_led, test led gpio);

#if defined(BOARD_USING_UART1_RS232)
#define NU_UART1_DEVNAME "uart1"

int test_rs232(int argc, char **argv)
{
    rt_device_t serial;
    char txbuf[16];
    rt_err_t ret;
    int str_len;

    serial = rt_device_find(NU_UART1_DEVNAME);
    if (!serial)
    {
        rt_kprintf("Can't find %s. EXIT.\n", NU_UART1_DEVNAME);
        goto exit_test_rs232;
    }

    rt_kprintf("Find %s.\n", NU_UART1_DEVNAME);

    /* Interrupt RX */
    ret = rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(ret == RT_EOK);

    rt_snprintf(&txbuf[0], sizeof(txbuf), "Hello World!\r\n");
    str_len = rt_strlen(txbuf);

    /* Say Hello */
    ret = rt_device_write(serial, 0, &txbuf[0], str_len);
    RT_ASSERT(ret == str_len);

    ret = rt_device_close(serial);
    RT_ASSERT(ret == RT_EOK);

exit_test_rs232:
    return 0;
}
MSH_CMD_EXPORT(test_rs232, test rs232 communication);

#define NU_UART_RS232_ASCII_DEVNAME NU_UART1_DEVNAME

// struct SER_PORT SER_PORTS[3] = {
//     {"uart1", 1, "ascii", RT_SERIAL_CONFIG_DEFAULT}, // ASCII-RS232 slave
//     {"uart6", 1, "ascii", RT_SERIAL_CONFIG_DEFAULT}, // ASCII-RS485 slave
//     {"uart8", 0, "rtu", RT_SERIAL_CONFIG_DEFAULT}};  // RTU-RS485 master

static char uart_get_char(rt_device_t serial_device, rt_sem_t rx_sem)
{
    char ch;

    /* 从串口读取一个字节的数据，没有读取到则等待接收信号量 
        第二个参数也可以是 -1 
        如果没有读取到，则返回 0
        否则返回 1
    */
    while (rt_device_read(serial_device, 0, &ch, 1) == 0)
    {
        rt_sem_control(rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        /* 阻塞等待接收信号量，等到信号量后再次读取数据 */
        rt_sem_take(rx_sem, RT_WAITING_FOREVER);
    }
    return ch;
}

#define DATA_CMD_END '\n' /* 结束位设置为 \r，即回车符 */


// int read_parse_config()
// {
//     int fd, size;
//     // char s[] = "RT-Thread Programmer!";
//     char buffer[80];

//     // rt_kprintf("Write string %s to test.txt.\n", s);

//     // /* 以创建和读写模式打开 CONFIG 文件，如果该文件不存在则创建该文件 */
//     // fd = open(CONFIG_FILE_PATH, O_WRONLY | O_CREAT);
//     // if (fd >= 0)
//     // {
//     //     write(fd, s, sizeof(s));
//     //     close(fd);
//     //     rt_kprintf("Write done.\n");
//     // }

//     /* 以只读模式打开 /text.txt 文件 */
//     fd = open(CONFIG_FILE_PATH, O_RDONLY);
//     if (fd >= 0)
//     {
//         size = read(fd, buffer, sizeof(buffer));
//         close(fd);
//         rt_kprintf("Read from file test.txt : %s \n", buffer);
//         if (size < 0)
//             return -1;
//     }

//     cJSON *root = NULL;
//     cJSON *item = NULL;

//     root = cJSON_Parse(buffer);

//     if (!root)
//     {
//         log_e("Error before:[%s]\n", cJSON_GetErrorPtr());
//     }
//     else
//     {
//         // log_d("formated print:%s", cJSON_Print(root));
//         // log_d("unformated print:%s", cJSON_PrintUnformatted(root));
//         item = cJSON_GetObjectItem(root, "masterID");
//         log_d("get masterID:%d", item->valueint);
//     }
// }



#endif //#defined(BOARD_USING_UART1_RS232)

#if defined(BOARD_USING_UART6_RS485)
#define NU_UART6_DEVNAME "uart6"
int test_uart6(int argc, char **argv)
{
    rt_device_t serial;
    char txbuf[32];
    rt_err_t ret;
    int str_len;
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT; /* 初始化配置参数 */

    serial = rt_device_find(NU_UART6_DEVNAME);
    if (!serial)
    {
        rt_kprintf("Can't find %s. EXIT.\n", NU_UART6_DEVNAME);
        goto exit_test_rs485;
    }

    rt_kprintf("Find %s.\n", NU_UART6_DEVNAME);

    config.baud_rate = BAUD_RATE_9600; //修改波特率为 115200
    config.data_bits = DATA_BITS_8;    //数据位 8
    config.stop_bits = STOP_BITS_1;    //停止位 1
    config.bufsz = 128;                //修改缓冲区 buff size 为 128
    config.parity = PARITY_NONE;       //无奇偶校验位

    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);

    /* Interrupt RX */
    ret = rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(ret == RT_EOK);

    /* Nuvoton private command  不设置这个也能用*/
    // nu_uart_set_rs485aud((struct rt_serial_device *)serial, RT_FALSE);

    int send_times = 3;
    while (send_times--)
    {
        rt_kprintf("send:%d\n", send_times);
        rt_snprintf(&txbuf[0], sizeof(txbuf), "Hello from uart6\r\n");
        str_len = rt_strlen(txbuf);

        /* Say Hello */
        ret = rt_device_write(serial, 0, txbuf, str_len);
        RT_ASSERT(ret == str_len);
    }

    ret = rt_device_close(serial);
    RT_ASSERT(ret == RT_EOK);

exit_test_rs485:
    return 0;
}
MSH_CMD_EXPORT(test_uart6, test rs485A communication);
#endif //defined(BOARD_USING_UART6_RS485)

#if defined(BOARD_USING_UART8_RS485)

#define NU_UART8_DEVNAME "uart8"

int test_uart8(int argc, char **argv)
{
    char txbuf[32];
    rt_err_t ret;
    int str_len;

    rt_device_t serial = rt_device_find(NU_UART8_DEVNAME);
    if (!serial)
    {
        rt_kprintf("Can't find %s. EXIT.\n", NU_UART8_DEVNAME);
        goto exit_test_rs485;
    }

    rt_kprintf("Find %s.\n", NU_UART8_DEVNAME);

    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT; /* 初始化配置参数 */

    config.baud_rate = BAUD_RATE_115200; //修改波特率为 115200
    config.data_bits = DATA_BITS_8;      //数据位 8
    config.stop_bits = STOP_BITS_1;      //停止位 1
    config.bufsz = 128;                  //修改缓冲区 buff size 为 128
    config.parity = PARITY_NONE;         //无奇偶校验位

    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);

    /* Interrupt RX */
    ret = rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(ret == RT_EOK);

    /* Nuvoton private command */
    nu_uart_set_rs485aud((struct rt_serial_device *)serial, RT_FALSE);

    int send_times = 3;
    while (send_times--)
    {
        rt_kprintf("send:%d\n", send_times);
        rt_snprintf(&txbuf[0], sizeof(txbuf), "Hello from uart8\r\n");
        str_len = rt_strlen(txbuf);

        /* Say Hello */
        ret = rt_device_write(serial, 0, &txbuf[0], str_len);
        RT_ASSERT(ret == str_len);
    }

    ret = rt_device_close(serial);
    RT_ASSERT(ret == RT_EOK);

exit_test_rs485:
    return 0;
}
MSH_CMD_EXPORT(test_uart8, test rs485 B communication);
#endif //defined(BOARD_USING_UART8_RS485)

static uint32_t u32Key1 = KEY_1;

void nu_button_cb(void *args)
{
    uint32_t u32Key = *((uint32_t *)(args));
    rt_kprintf("\nbutton click:%d\n", u32Key);
    switch (u32Key)
    {
    case KEY_1:
        rt_pin_write(LED_2, ~rt_pin_read(LED_2));
        extern void rt_hw_cpu_reset(void);
        rt_hw_cpu_reset();
        break;
        // case KEY_2:
        //     rt_kprintf("4G Status change!\n");
        //     break;
    }
}

int board_init(void)
{

    rt_kprintf("Begin to init LED!\n");

    rt_pin_mode(LED_1, PIN_MODE_OUTPUT);
    rt_pin_mode(LED_2, PIN_MODE_OUTPUT);

    rt_pin_write(LED_1, PIN_LOW);
    rt_pin_write(LED_2, PIN_LOW);

    led_1 = easyblink_init_led(LED_1, PIN_LOW);
    led_2 = easyblink_init_led(LED_2, PIN_LOW);

    easyblink(led_1, -1, 250, 500);
    easyblink(led_2, 10, 250, 500);

    rt_kprintf("Begin to init KEY!\n");

    rt_pin_mode(KEY_1, PIN_MODE_INPUT_PULLUP);

    rt_pin_attach_irq(KEY_1, PIN_IRQ_MODE_FALLING, nu_button_cb, &u32Key1);
    rt_pin_irq_enable(KEY_1, PIN_IRQ_ENABLE);

    rt_kprintf("Begin to init ASCII PORTS\n");

    // 读取 config.json 到 g_BUF_CONFIG_JSON

    // msh_exec("ls /mnt/filesystem/webnet",26);

}

// INIT_BOARD_EXPORT 这个不能用在这里初始化板子，会出问题,
// INIT_DEVICE_EXPORT(board_init);
INIT_APP_EXPORT(board_init);
