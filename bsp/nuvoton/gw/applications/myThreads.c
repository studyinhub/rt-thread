#include "rtconfig.h"
#include "rtthread.h"
#include <string.h>
#include <stdio.h> // sprintf
#include <rtdevice.h>

#include "board.h"
#include "myThreads.h"
#include "modbus_x.h"
#include "myConfig.h"

#define LOG_TAG "mythread"
#define LOG_LVL LOG_LVL_DBG // LOG_LVL_INFO
#include <ulog.h>

#define THREAD_TIMESLICE 5
#define THREAD_PRIORITY 10
#define THREAD_STACK_SIZE 1024

#define MB_POLL_THREAD_PRIORITY RT_THREAD_PRIORITY_MAX - 1

// https://blog.csdn.net/lu_embedded/article/details/107308740
#define RING_BUFFER_LEN 512
static struct rt_ringbuffer *rb;
// static struct rt_mailbox mb; // 邮箱：用来保存 存储到 ringbufer 中的帧长度，当收到 ASCII 帧并送入 ringbuffer 后，就发送邮件，内容为此次接收帧的长度
// static char mb_pool[128];    /* 用于放邮件的内存池 */

#define BYTES_PER_MSG 128

// ASCII 接收消息队列
static struct rt_messagequeue asc_recv_mq;
static rt_uint8_t asc_recv_pool[2048];

#define BYTES_PER_SEND_MSG 1024

// ASCII 响应消息队列
struct rt_messagequeue asc_send_mq;
// 队列池，此池决定了，能够缓存的消息数量 4096 ，实测最多三条消息
static rt_uint8_t asc_send_pool[40960];

// ASCII 响应消息邮箱
struct rt_mailbox asc_resp_mb;
static rt_uint8_t asc_resp_mb_pool[128];

// RTU 接收响应消息队列
static struct rt_messagequeue rtu_recv_mq;
static rt_uint8_t rtu_recv_pool[2048];

ALIGN(RT_ALIGN_SIZE)
static char thread_asc_stack[2048];
static struct rt_thread thread_asc;

ALIGN(RT_ALIGN_SIZE)
static char thread_asc_resp_stack[2048];
static struct rt_thread thread_asc_resp;

ALIGN(RT_ALIGN_SIZE)
static char thread_rtu_stack[1024];
static struct rt_thread thread_rtu;

static void thread_asc_entry(void *parameter)
{
    rt_uint16_t error_count = 0;

    LOG_I("thread_asc_entry");
    struct SER_MSG ser_msg; /* 用于放置消息的局部变量 */
    struct SER_PORT *resp_port;

    ser_msg.data_ptr = rt_malloc(27 * 100);

    while (1)
    {
        while (rt_mq_recv(&asc_recv_mq, &ser_msg, sizeof(struct SER_MSG), RT_WAITING_FOREVER) == RT_EOK)
        {

            // if(ser_msg.data_size != rt_strlen(ser_msg.data_ptr))
            // {
            //     log_e(":%d,%d",ser_msg.data_size,rt_strlen(ser_msg.data_ptr));
            //     continue;
            // }

            // log_d("mq buf(%d):%s",ser_msg.data_size,ser_msg.data_ptr);
            
            // :010300000007F5
            // :011700040003000B000204009B000134
            if (ser_msg.data_size >= 8) //27
            {
                parse_ascii_frame(ser_msg.data_ptr, ser_msg.data_size);
            }else{
                log_d("ser_msg:%d",ser_msg.data_size);
            }
        }
    }

    free(ser_msg.data_ptr);
}

// 由原来邮箱改为消息队列处理发送
static void thread_asc_resp_entry(void *parameter)
{
    rt_uint16_t error_count = 0;

    LOG_I("thread_asc_resp_entry");
    struct SER_MSG ser_msg; /* 用于放置消息的局部变量 */
    struct SER_PORT *resp_port = RT_NULL;
    clock_t start = 0, end = 0;
    uint16_t cycles = 0;
    uint16_t write_len = 0;
    while (1)
    {

        // 取邮件内容
        if (rt_mq_recv(&asc_send_mq, &ser_msg, sizeof(struct SER_MSG), RT_WAITING_FOREVER) == RT_EOK) // RT_TICK_PER_SECOND RT_WAITING_FOREVER
        {
            // 通过邮箱获取到请求的端口,取邮件地址
            if (rt_mb_recv(&asc_resp_mb, (rt_ubase_t *)&resp_port, 10) == RT_EOK) // 10ms
            {
                // log_d("%s", resp_port->dev_name);
            }

            if (ser_msg.data_size > 0 && resp_port != RT_NULL)
            {

                // start = clock();
                write_len = rs485_send(resp_port->device, ser_msg.data_ptr, ser_msg.data_size);
                // end = clock();
                // cycles = end-start;
                // do
                // {
                //     rt_pin_write(LED_RUN, !(rt_pin_read(LED_RUN)));
                //     rt_thread_mdelay(1);
                // }while(cycles--);

                // rt_pin_write(LED_RUN, LED_OFF);

                if (write_len != ser_msg.data_size)
                {
                    log_e("发送失败");
                }
                else
                {
                    log_i("%s->%d %d %s",resp_port->dev_name,asc_send_mq.entry,asc_resp_mb.entry,ser_msg.data_ptr);
                    rt_memset(ser_msg.data_ptr, 0, ser_msg.data_size);
                    ser_msg.data_size = 0;
                }
            }

            rt_uint16_t nums = 1;
            if (write_len >= 100)
                nums = 1;
            else
            {
                nums = 2;
            }
            easyblink(led_run, nums, 50, 100);
        }

        if (asc_send_mq.entry == 0)
        {
            while (asc_resp_mb.entry > 0)
            {
                log_w("clear asc_resp_mb");
                if (rt_mb_recv(&asc_resp_mb, (rt_ubase_t *)&resp_port, 1 * RT_TICK_PER_SECOND) == RT_EOK)
                {
                    log_w("resp_port->dev_name:%s clear mailbox", resp_port->dev_name);
                }
            }
        }

        // rt_thread_mdelay(1000);// 延时 1s
    }
}

static void thread_rtu_entry(void *parameter)
{

    LOG_I("thread_rtu_entry");
    rt_uint8_t *ptrFrame;
    rt_uint32_t frame_len = 0;

    struct SER_MSG ser_msg; /* 用于放置消息的局部变量 */
    while (1)
    {
        if (rt_mq_recv(&rtu_recv_mq, &ser_msg, sizeof(struct SER_MSG), RT_WAITING_FOREVER) == RT_EOK)
        {
            // log_d("mq len:%d",ser_msg.data_size);
            // for (int i = 0; i < ser_msg.data_size; i++)
            // {
            //     rt_kprintf("%02X ", *(ser_msg.data_ptr + i));
            // }
            // rt_kprintf("\n");

            // parse_rtu_frame(ser_msg.data_ptr, ser_msg.data_size);
        }
    }
}

void serial_thread_entry(void *parameter)
{
    int result;
    char ch;
    int index = (int)parameter, i = 0, j = 0;
    log_d("%s read thread started\n", g_stConfig.serPort[index].dev_name);

    int ReadDatStAdd;  //上位机要的读的数据起始地
    int ReadDatLenth;  //上位机要读的数据的长度
    int WriteDatStAdd; //上位机要写的数据的起始地址
    int WriteDatLenth; //上位机要写的数据的长度
    int WriteByteNum;  //上位机要写的数据的字节长度=数据长度*2
    uint8_t slaveAddr; //上位机请求中的从机地址
    uint8_t funcode;   //上位机请求中的功能码
    uint8_t lrc;       //上位机请求中的 LRC

    static int f_index = 0;
    clock_t start = 0, end = 0;
    int readlen = 0;
    int buflen = 0;
    uint8_t calc_lrc = 0;

    struct SER_PORT *port = &g_stConfig.serPort[index];

    char *prBuf = port->rx_buf;
    char *pwBuf = port->tx_buf;

    struct SER_MSG ser_msg;

    while (1)
    {
        // 等待数据接收完成
        rt_sem_control(&g_stConfig.serPort[index].rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        /* 阻塞等待接收信号量，等到中断后再次读取数据 */
        rt_sem_take(&g_stConfig.serPort[index].rx_sem, RT_WAITING_FOREVER);

        start = clock();
        // 从第1个字节开始计时，超过 10ms，就终止本次读取
        while (1)
        {
            if (port->CanRecv <= 1)
                port->CanRecv = 1;

            // rt_kprintf("CanRecv:%d\n", port->CanRecv);
            readlen = rt_device_read(port->device, 0, prBuf + MAX_BUF_LENGTH - port->CanRecv, port->CanRecv);
            // rt_kprintf("readlen:%d\n", readlen);
            if (readlen > 0)
            {
                start = clock(); //收到数据，重新开始计时，返回值单位：毫秒
                port->CanRecv = port->CanRecv - readlen;
            }
            else
            {
                end = clock();
                rt_thread_delay(1);
                // rt_kprintf("end-start:%d\r\n",end-start);
                if ((end - start) > g_stConfig.serPort[index].frameInterval)
                {
                    // log_w("%s一帧读取完毕:%d ", port->dev_name, end - start);
                    break;
                }
            }
        }

        buflen = MAX_BUF_LENGTH - port->CanRecv; //接收到的总字节数

        ser_msg.data_ptr = prBuf;
        ser_msg.data_size = buflen;
        ser_msg.port = port;

        if (!rt_strcmp(port->prot, "ascii"))
        // if(index >=1)
        {
            // log_d("ASCII 口收到数据:%d", buflen);

            *(prBuf + buflen) = '\0';

            // rt_kprintf("buf:%s", prBuf);
            // rt_ringbuffer_put(rb, prBuf, buflen);

            /* 发送消息到消息队列中 */
            // result = rt_mq_send(&asc_recv_mq, prBuf, buflen);
            result = rt_mq_send(&asc_recv_mq, &ser_msg, sizeof(struct SER_MSG));
            if (result != RT_EOK)
            {
                log_e("rt_mq_send asc_recv_mq ERR\n");
            }

            /* 发送串口消息到 asc_resp_mq，告诉 RTU 是谁在请求，响应的时候就响应给谁*/
            // log_i("serport index:%s",port->dev_name);
            // log_i("port ddr:0x%08X",port);
            // log_i("port ddr1:0x%08X",&g_stConfig.serPort[index]);
            // if(rt_mb_send(&asc_resp_mb, (struct SER_PORT *)port))

            // 这里还是要发送普通邮件，不能是紧急邮件，如果解析失败，需要将邮件扔掉
            if (rt_mb_send(&asc_resp_mb, (rt_uint32_t)port))
            {

                log_e("ASCII 接收线程邮件发送失败\n");
            }

            // 这里加一个延时会降低速度，主要是等待解析线程解析完再去清理 rx_buf
            // 优化方案，可以通过接收解析线程的邮件来处理，达到一个线程间同步的问题
            // rt_thread_delay(1000);
        }
        else
        {
            // RTU 口收到数据
            log_d("RTU 口收到数据:%d", buflen);

            // RTU 收到的数据是 hex 并不是所以不能直接按照字符串来打印
            // ulog_hexdump("rtu_recv",16,prBuf,buflen);
            // for (int j = 0; j < buflen; j++)
            // {
            //     rt_kprintf("%02X ", *(prBuf + j));
            // }
            // rt_kprintf("\n");

            if (rt_mq_send(&rtu_recv_mq, &ser_msg, sizeof(struct SER_MSG)) != RT_EOK)
            {
                log_e("rt_mq_send rtu_recv_mq ERR\n");
            }
        }
        // 不需要清理，直接覆盖即可
        // memset(port->rx_buf, 0, MAX_BUF_LENGTH);
        port->CanRecv = MAX_BUF_LENGTH;
    }
}

#include "agile_modbus.h"

static struct rt_timer timer1;
static int cnt = 0;

static void cb_timer1(void *parameter)
{

    log_d("Timer");
}

int threads_init(void)
{
    rt_err_t result = RT_EOK;

    // /* 初始化一个 mailbox */
    // result = rt_mb_init(&mb,
    //                     "mbt",               /* 名称是 mbt */
    //                     &mb_pool[0],         /* 邮箱用到的内存池是 mb_pool */
    //                     sizeof(mb_pool) / 4, /* 邮箱中的邮件数目，因为一封邮件占 4 字节 */
    //                     RT_IPC_FLAG_FIFO);   /* 采用 FIFO 方式进行线程等待 */

    rb = rt_ringbuffer_create(RING_BUFFER_LEN);
    if (rb == RT_NULL)
    {
        log_e("Can't create ringbffer");
        return -1;
    }

    /* 初始化消息队列 
        asc_recv_mq 接收消息队列，接收到的消息都存储到此队列中，也就是生产着
        等待解析线程进行消费
        解析线程消费完了之后，要将消息来源发送到等待消息发送队列 asc_resp_mq 中。
        当 rtu_recv_mq 接收消息队列收到数据后，要消费 asc_resp_mq 中的消息，并根据消息来源，执行响应。
    */
    result = rt_mq_init(&asc_recv_mq,
                        "mq_1",
                        &asc_recv_pool[0],     /* 内存池指向 msg_pool */
                        BYTES_PER_MSG,         /* 每个消息的大小是 128 字节 */
                        sizeof(asc_recv_pool), /* 内存池的大小是 msg_pool 的大小 */
                        RT_IPC_FLAG_FIFO);     /* 如果有多个线程等待，优先级大小的方法分配消息*/

    if (result != RT_EOK)
    {
        log_e("init asc_recv_mq queue failed.\n");
        return -1;
    }

    result = rt_mq_init(&asc_send_mq,
                        "mq_2",
                        &asc_send_pool[0],     /* 内存池指向 msg_pool */
                        BYTES_PER_SEND_MSG,    /* 每个消息的大小是 128 字节 */
                        sizeof(asc_send_pool), /* 内存池的大小是 msg_pool 的大小 */
                        RT_IPC_FLAG_FIFO);     /* 如果有多个线程等待，优先级大小的方法分配消息*/

    if (result != RT_EOK)
    {
        log_e("init asc_send_mq queue failed.\n");
        return -1;
    }

    result = rt_mb_init(&asc_resp_mb,
                        "mb",
                        &asc_resp_mb_pool[0],         /* 内存池指向 msg_pool */
                        sizeof(asc_resp_mb_pool) / 4, /* 每个消息的大小是 128 字节 */
                        RT_IPC_FLAG_FIFO);            /* 如果有多个线程等待，优先级大小的方法分配消息*/

    if (result != RT_EOK)
    {
        log_e("init asc_resp_mb mailbox failed.\n");
        return -1;
    }

    result = rt_mq_init(&rtu_recv_mq,
                        "mq_3",
                        &rtu_recv_pool[0],     /* 内存池指向 msg_pool */
                        BYTES_PER_MSG,         /* 每个消息的大小是 128 字节 */
                        sizeof(rtu_recv_pool), /* 内存池的大小是 msg_pool 的大小 */
                        RT_IPC_FLAG_FIFO);     /* 如果有多个线程等待，优先级大小的方法分配消息*/

    if (result != RT_EOK)
    {
        log_e("init rtu_recv_mq queue failed.\n");
        return -1;
    }

    rt_thread_init(&thread_asc,
                   "thread_asc",
                   thread_asc_entry,
                   RT_NULL,
                   &thread_asc_stack[0],
                   sizeof(thread_asc_stack), THREAD_PRIORITY, THREAD_TIMESLICE);

    rt_thread_startup(&thread_asc);

    rt_thread_init(&thread_asc_resp,
                   "thread_asc_resp",
                   thread_asc_resp_entry,
                   RT_NULL,
                   &thread_asc_resp_stack[0],
                   sizeof(thread_asc_resp_stack), THREAD_PRIORITY, THREAD_TIMESLICE);

    rt_thread_startup(&thread_asc_resp);

    // rt_thread_init(&thread_rtu,
    //                "thread_rtu",
    //                thread_rtu_entry,
    //                RT_NULL,
    //                &thread_rtu_stack[0],
    //                sizeof(thread_rtu_stack), THREAD_PRIORITY, THREAD_TIMESLICE);

    // rt_thread_startup(&thread_rtu);

    // rt_timer_init(&timer1,"tm1",cb_timer1,RT_NULL,RT_TICK_PER_SECOND,RT_TIMER_FLAG_PERIODIC);
    // rt_timer_start(&timer1);

    // // 轮询数据线程
    // rt_thread_t thread_mbsend = rt_thread_create("md_m_send", ascii_thread_entry, RT_NULL, 512, MB_POLL_THREAD_PRIORITY, 10);
    // if (thread_mbsend != RT_NULL)
    // {
    //     rt_thread_startup(thread_mbsend);
    // }

    // sprintf(thread_name, "mbp_%s", g_stConfig.serPort[i].dev_name);
    // log_d("This port (%s) is rtu,will as rtu master", g_stConfig.serPort[i].dev_name);
    // rt_thread_t thread_mbpoll = rt_thread_create(thread_name, (void (*)(void *parameter))mbpoll_thread_entry, (void *)i, 1024, MB_POLL_THREAD_PRIORITY, 10);
    // if (thread_mbpoll != RT_NULL)
    // {
    //     rt_thread_startup(thread_mbpoll);
    // }

    return 0;
}

void detach_threads()
{
}

// void mbpoll_parese_entry(void *parameter)
// {
//     // eMBMasterReqErrCode error_code = MB_MRE_NO_ERR;
//     // USHORT data[2] = {0};
//         // /* Test Modbus Master */
//         // data[0] = (USHORT)(rt_tick_get() / 10);
//         // data[1] = (USHORT)(rt_tick_get() % 10);

//         // error_code = eMBMasterReqWriteMultipleHoldingRegister(g_stConfig.slaveAddr,    /* salve address */
//         //                                                       g_stConfig.startRegAddr, /* register start address */
//         //                                                       g_stConfig.regsCnt,      /* register total number */
//         //                                                       data,                /* data to be written */
//         //                                                       RT_WAITING_FOREVER); /* timeout */

//         // eMBMasterReqReadHoldingRegister(g_stConfig.slaveAddr,
//         //                                 g_stConfig.startRegAddr,
//         //                                 g_stConfig.regsCnt,
//         //                                 10);

//         // for (int i = 0; i < g_stConfig.regsCnt; i++)
//         // {
//         //     rt_kprintf("usMRegHoldBuf[%d]:%d\n", i, usMRegHoldBuf[0][i]);
//         // }

//         // /* Record the number of errors */
//         // if (error_code != MB_MRE_NO_ERR)
//         // {
//         //     error_count++;
//         // }
// }
// #define MB_POLL_CYCLE_MS 10
// static void mbpoll_thread_entry(void *parameter)
// {
//     eMBMasterInit(MB_RTU, MB_MASTER_USING_PORT_NUM, MB_MASTER_USING_PORT_BAUDRATE, MB_PAR_NONE);
//     eMBMasterEnable();
//     while (1)
//     {
//         eMBMasterPoll();
//         rt_thread_mdelay(MB_POLL_CYCLE_MS);
//     }
// }