#include "rtconfig.h"
#include "rtthread.h"
#include <string.h>
#include <stdio.h> // sprintf
#include <rtdevice.h>
#include "myThreads.h"
#include "modbus_x.h"
#include "config.h"

#define LOG_TAG "mythread"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

#define MB_POLL_THREAD_PRIORITY RT_THREAD_PRIORITY_MAX - 1

// https://blog.csdn.net/lu_embedded/article/details/107308740
#define RING_BUFFER_LEN 512
static struct rt_ringbuffer *rb;

// 邮箱：用来保存 存储到 ringbufer 中的帧长度，当收到 ASCII 帧并送入 ringbuffer 后，就发送邮件，内容为此次接收帧的长度
static struct rt_mailbox mb;
/* 用于放邮件的内存池 */
static char mb_pool[128];

static void poll_thread_entry(void *parameter)
{
    // eMBMasterReqErrCode error_code = MB_MRE_NO_ERR;
    rt_uint16_t error_count = 0;
    // USHORT data[2] = {0};

    LOG_I("poll_thread_entry");
    rt_size_t rb_len;
    rt_uint32_t frame_len = 0;

    while (1)
    {

        if (rt_mb_recv(&mb, (rt_ubase_t *)&frame_len, RT_WAITING_FOREVER) == RT_EOK)
        {
            log_d("recv frame_len:%d", frame_len);
        }

        rb_len = rt_ringbuffer_data_len(rb);
        log_d("consumer:%d", rb_len);

        rt_uint8_t *ptrFrame;
        ptrFrame = rt_malloc(frame_len);
        rt_ringbuffer_get(rb, ptrFrame, frame_len);

        // ptrFrame[frame_len] = '\0';
        // log_d("frame:%s", ptrFrame);

        for (int i = 0; i < frame_len; i++)
        {
            rt_kprintf("%c", *(ptrFrame + i));
        }

        parse_ascii_frame(ptrFrame, frame_len);
        rb_len = rt_ringbuffer_data_len(rb);
        log_d("consumered:%d", rb_len);

        rt_free(ptrFrame);

        // /* Test Modbus Master */
        // data[0] = (USHORT)(rt_tick_get() / 10);
        // data[1] = (USHORT)(rt_tick_get() % 10);

        // error_code = eMBMasterReqWriteMultipleHoldingRegister(g_stConfig.slaveAddr,    /* salve address */
        //                                                       g_stConfig.startRegAddr, /* register start address */
        //                                                       g_stConfig.regsCnt,      /* register total number */
        //                                                       data,                /* data to be written */
        //                                                       RT_WAITING_FOREVER); /* timeout */

        // eMBMasterReqReadHoldingRegister(g_stConfig.slaveAddr,
        //                                 g_stConfig.startRegAddr,
        //                                 g_stConfig.regsCnt,
        //                                 10);

        // for (int i = 0; i < g_stConfig.regsCnt; i++)
        // {
        //     rt_kprintf("usMRegHoldBuf[%d]:%d\n", i, usMRegHoldBuf[0][i]);
        // }

        // /* Record the number of errors */
        // if (error_code != MB_MRE_NO_ERR)
        // {
        //     error_count++;
        // }
    }
}

void serial_thread_entry(void *parameter)
{
    char ch;
    int index = (int)parameter, i = 0, j = 0;
    LOG_D("%s read thread started\n", g_stConfig.serPort[index].dev_name);

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
            // : 01 17 0004 0003 000B 0002 04 009B 0001 34
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
                if ((end - start) > 5) //间隔超时 5ms
                {
                    log_w("一帧读取完毕:%d ", end - start);
                    break;
                }
            }
        }

        buflen = MAX_BUF_LENGTH - port->CanRecv; //接收到的总字节数

        if (!rt_strcmp(port->prot, "ascii"))
        {
            log_i("ASCII 口收到数据:%d", buflen);
            rt_kprintf("buf:%s", prBuf);
            rt_ringbuffer_put(rb, prBuf, buflen);
            rt_mb_send(&mb, buflen);
        }
        else
        {
            // RTU 口收到数据
            log_i("RTU 口收到数据:%d", buflen);
            // RTU 收到的数据是 hex 并不是所以不能直接按照字符串来打印
            for (int j = 0; j < buflen; j++)
            {
                rt_kprintf("%02X ", *(prBuf + j));
            }
            rt_kprintf("\n");

            parse_rtu_frame(prBuf, buflen);
        }
        memset(port->rx_buf, 0, MAX_BUF_LENGTH);
        port->CanRecv = MAX_BUF_LENGTH;
    }
}


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


int threads_init(void)
{
    rt_err_t result = RT_EOK;
     /* 初始化一个 mailbox */
    result = rt_mb_init(&mb,
                        "mbt",               /* 名称是 mbt */
                        &mb_pool[0],         /* 邮箱用到的内存池是 mb_pool */
                        sizeof(mb_pool) / 4, /* 邮箱中的邮件数目，因为一封邮件占 4 字节 */
                        RT_IPC_FLAG_FIFO);   /* 采用 FIFO 方式进行线程等待 */

    rb = rt_ringbuffer_create(RING_BUFFER_LEN);
    if (rb == RT_NULL)
    {
        log_e("Can't create ringbffer");
        return -1;
    }

    // 轮询数据线程
    rt_thread_t thread_mbsend = rt_thread_create("md_m_send", poll_thread_entry, RT_NULL, 512, MB_POLL_THREAD_PRIORITY, 10);
    if (thread_mbsend != RT_NULL)
    {
        rt_thread_startup(thread_mbsend);
    }
    
    // sprintf(thread_name, "mbp_%s", g_stConfig.serPort[i].dev_name);
    // LOG_D("This port (%s) is rtu,will as rtu master", g_stConfig.serPort[i].dev_name);
    // rt_thread_t thread_mbpoll = rt_thread_create(thread_name, (void (*)(void *parameter))mbpoll_thread_entry, (void *)i, 1024, MB_POLL_THREAD_PRIORITY, 10);
    // if (thread_mbpoll != RT_NULL)
    // {
    //     rt_thread_startup(thread_mbpoll);
    // }

}