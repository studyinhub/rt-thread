#include "myThreads.h"
#include "board.h"
#include "modbus_x.h"
#include "myConfig.h"
#include "rtconfig.h"
#include "rtdef.h"
#include "rtthread.h"
#include <rtdevice.h>
#include <sys/types.h>
#include <ipc/ringbuffer.h>

// #include <stdio.h> // sprintf
// #include <string.h>

#include "utils.h"

#define LOG_TAG "mythread"
// #define LOG_LVL LOG_LVL_ERROR // LOG_LVL_INFO
// #define LOG_LVL LOG_LVL_WARNING
#define LOG_LVL LOG_LVL_DBG // LOG_LVL_INFO
#include <ulog.h>

// https://blog.csdn.net/lu_embedded/article/details/107308740
// #define RING_BUFFER_LEN 512
// static struct rt_ringbuffer *rb;
// static struct rt_mailbox mb; // 邮箱：用来保存 存储到 ringbufer
// 中的帧长度，当收到 ASCII 帧并送入 ringbuffer
// 后，就发送邮件，内容为此次接收帧的长度 static char mb_pool[128];    /*
// 用于放邮件的内存池 */

// ASCII 接收消息队列
#define ASC_RECV_MSG_SIZE 2048 
// #define ASC_SEND_MSG_SIZE 1024
#define ASC_RECV_MQ_POOL 5120
// #define ASC_SEND_MQ_POOL 40960
#define RTU_SEND_MSG_SIZE 1024
#define RTU_SEND_MQ_POOL 40960

#define MAX_ASC_RECV_MSG_QUANTITY ASC_RECV_MQ_POOL / ASC_RECV_MSG_SIZE
// #define MAX_ASC_SEND_MSG_QUANTITY ASC_SEND_MQ_POOL/ASC_SEND_MSG_SIZE
#define MAX_RTU_SEND_MSG_QUANTITY RTU_SEND_MQ_POOL / RTU_SEND_MSG_SIZE

// ASCII 响应消息队列
// 队列池，此池决定了，能够缓存的消息数量 4096 ，实测最多三条消息
// RTU 接收响应消息队列
struct rt_messagequeue asc_recv_mq;
// static struct rt_messagequeue asc_send_mq;
struct rt_messagequeue rtu_send_mq;
static rt_uint8_t asc_recv_pool[ASC_RECV_MQ_POOL]; // 5120/128 = 40
// static rt_uint8_t asc_send_pool[ASC_SEND_MQ_POOL]; // 40960/1024 =40
static rt_uint8_t rtu_send_pool[RTU_SEND_MQ_POOL];

// ASCII 响应消息邮箱
// struct rt_mailbox asc_resp_mb;
// 邮箱容量是 128/4=32
// static rt_uint8_t asc_resp_mb_pool[MAILBOX_POOL_SIZE];

// #define MB_POLL_THREAD_PRIORITY RT_THREAD_PRIORITY_MAX - 1

ALIGN(RT_ALIGN_SIZE)
// static char thread_asc_stack[2048];
static char thread_asc_stack[8972];
static struct rt_thread thread_asc;

ALIGN(RT_ALIGN_SIZE)
static char thread_asc_resp_stack[2048];
static struct rt_thread thread_asc_resp;

// ALIGN(RT_ALIGN_SIZE)
// static char thread_rtu_stack[1024];
// static struct rt_thread thread_rtu;

ALIGN(RT_ALIGN_SIZE)
static char thread_rtu_master_stack[1024];
static struct rt_thread thread_rtu_master;


rt_mutex_t dynamic_mutex = RT_NULL;


struct RTU_WRITE {
  uint16_t start_addr;
  uint16_t wrRegQuantity;
  uint16_t data[AGILE_MODBUS_MAX_WRITE_REGISTERS];
};


static void thread_rtu_master_entry(void *parameter) {

  int i = 0, got_ascAddr = 0, cnt = 0, send_len = 0, read_len = 0, rc = 0;

  rtu_master_init();

  g_stConfig.rtuSys.hold =
      rt_malloc(sizeof(uint16_t) * g_stConfig.rtuSys.scanRegCnt);

  rt_memset(g_stConfig.rtuSys.hold, 0,
            sizeof(uint16_t) * g_stConfig.rtuSys.scanRegCnt);


  rt_err_t ret = RT_EOK;

  rt_tick_t start_tick = rt_tick_get();
  rt_tick_t timeout_tick = rt_tick_from_millisecond(1000);
  rt_tick_t timeout_tick_5 = rt_tick_from_millisecond(5000);
  rt_thread_delay(100);
  uint16_t* D90 = &g_stConfig.rtuSys.hold[90];
  uint16_t* D91 = &g_stConfig.rtuSys.hold[91];
  uint16_t* D92 = &g_stConfig.rtuSys.hold[92];

  do {
    ret = modbus_read_regs(g_ctx, g_stConfig.rtuSys.scanStAddr,
                                    g_stConfig.rtuSys.scanRegCnt,
                                    (uint16_t *)g_stConfig.rtuSys.hold,300);
    rt_thread_mdelay(100);
  }while(*D90 ==0 && *D91 ==0 && *D92 ==0);

  while (1) {

    // 总扫描开关
    if (!g_stConfig.rtuSys.scanEnable) {
      rt_thread_delay(10);
      continue;
    }
    
    if(rtu_send_mq.entry)
    {
      struct RTU_WRITE rtu_write;
      ret = rt_mq_recv(&rtu_send_mq, &rtu_write, sizeof(struct RTU_WRITE),0);

      if(rtu_send_mq.entry>=1)
      log_w("rtu_send_mq(%d/%d): %d %d",rtu_send_mq.entry,rtu_send_mq.max_msgs,
            rtu_write.start_addr,rtu_write.wrRegQuantity);


      ret = modbus_write_regs(g_ctx, rtu_write.start_addr,rtu_write.wrRegQuantity, rtu_write.data,100);

      if (ret != RT_EOK) {
        log_e("modbus_write_regs error");
      }
    }

    if(rtu_send_mq.entry==0 && rt_tick_get() - start_tick >= timeout_tick){
      
      start_tick = rt_tick_get();
      ret = modbus_read_regs(g_ctx, g_stConfig.rtuSys.scanStAddr,
                                      g_stConfig.rtuSys.scanRegCnt,
                                      (uint16_t *)g_stConfig.rtuSys.hold,300);

      if (ret != RT_EOK) {
        log_e("modbus_read_regs error");
      }
    }

  }
}

// #define TEST
//
//
//


void clear_message_queue(rt_mq_t mq)
{
    rt_err_t result;
    void *msg = RT_NULL;  // 用于接收消息的缓冲区，如果不关心内容可以随意

    /* 循环接收，直到队列为空 */
    while (1)
    {
        result = rt_mq_recv(mq, &msg, sizeof(void *), 0);  // 0 表示不等待，立即返回

        if (result == RT_EOK)
        {
            // 成功接收到一条消息，你可以选择处理它或者直接丢弃
            // 这里我们只是丢弃，不做处理
            // 如果 msg 是动态分配的内存，你可能需要释放它！
        }
        else if (result == -RT_ETIMEOUT || result == -RT_EEMPTY)
        {
            // 队列已经空了，退出循环
            // 注意：在 RT-Thread 新版本中，空队列时返回的是 -RT_EEMPTY
            break;
        }
        else
        {
            // 其他错误，可根据需要处理
            rt_kprintf("rt_mq_recv error: %d\n", result);
            break;
        }
    }

    // rt_kprintf("Message queue cleared.\n");
}

static void thread_asc_entry(void *parameter) {
  rt_uint16_t error_count = 0;
  LOG_I("thread_asc_entry");

  struct SER_MSG *ser_msg; 
  clock_t start = 0, end = 0;
  // log_d("mq buf(%d)",ser_msg->data_size);
  //
  rt_err_t ret;
  while (1) {
    // if msg queue empty then go await
    if (!asc_recv_mq.entry) {
      rt_thread_mdelay(10);
      continue;
    }

    // rt_mutex_take(dynamic_mutex, RT_WAITING_FOREVER);
    // log_d("asc_recv_mq.entry:%d/%d size:%d", asc_recv_mq.entry,asc_recv_mq.max_msgs, asc_recv_mq.msg_size);
    // log_d("data_ptr:%d res_ptr:%d", ser_msg.data_ptr, ser_msg.res_ptr);

    ret = rt_mq_recv(&asc_recv_mq, (rt_ubase_t*)&ser_msg, sizeof ser_msg,RT_WAITING_FOREVER);

    if (ret != RT_EOK) {
      if (ret == -RT_ETIMEOUT) {
        log_e("rt_mq_recv timeout");
      } else if (ret == -RT_ERROR) {
        log_e("rt_mq_recv error");
      }
      goto _exit;
    }


    log_d("!!!!!!!ser_msg->req_data:%s",ser_msg->req_data);
     // if (ret != RT_EOK) {
     //    if (ret == -RT_EFULL) {
     //      log_e("ASCII 接收线程消息队列已满,请清空");
     //    } else if (ret == -RT_ERROR) {
     //      log_e("msg maybe too large than max_msgs");
     //    } else {
     //      log_e("mq send to asc_resp_mq error unkown");
     //    }
     //    continue;
     //  }


    // log_e("asc_recv_mq(%d/%d)(%d):%s",asc_recv_mq.entry,asc_recv_mq.max_msgs,ser_msg.data_size,ser_msg.data_ptr);

    // clear_message_queue(&asc_recv_mq);
    // log_i("data_ptr:%d res_ptr:%d", ser_msg.data_ptr, ser_msg.res_ptr);
    // if (LOG_LVL == LOG_LVL_DBG) {
    //   log_d("ser_msg.data_size:%d port:%s", ser_msg.data_size,
    //         ser_msg.port->dev_name);
    //   ulog_hexdump("asc_recv", 16, ser_msg.data_ptr, ser_msg.data_size);
    // }

    // if(ser_msg.data_size != rt_strlen(ser_msg.data_ptr))
    // {
    //     log_e(":%d,%d",ser_msg.data_size,rt_strlen(ser_msg.data_ptr));
    //     continue;
    // }

    // log_e("mq buf(%d):%s",ser_msg.data_size,ser_msg.data_ptr);

    // log_d("ascii_parse(%d):",ser_msg.data_size);
    // if (LOG_LVL == LOG_LVL_DBG) {
    //   for (int i = 0; i < ser_msg.data_size; i++) {
    //     rt_kprintf("%c", *(ser_msg.data_ptr + i));
    //   }
    // }

    // log_i("data_ptr:%d res_ptr:%d", ser_msg.data_ptr, ser_msg.res_ptr);
    // start = clock();
    //
    ret = parse_serial_frame(ser_msg);

    // end = clock();
    // log_d("解析完毕,用时:%dms", end - start);
    if (ret != 1 && ret != 2) {
      log_e("parse error for :%s",ser_msg->req_data);
      ulog_hexdump("error data:", 16, ser_msg->req_data, rt_strlen(ser_msg->req_data));
      // uart_flush_rx(ser_msg->port->device);
      goto _exit;
    }

    rt_memset(ser_msg->res_data, '\0', 256);
    if(ret == 1)
      ret = ascii_build_response(ser_msg);
    else{
      ret = chct_build_response(ser_msg);
    }

    // print_asc_frame_meta(&ser_msg.meta);

    if (ret != RT_EOK) {
      log_e("ascii_build_response error,no send:");
      if (ret == RT_ENOSYS) {
        log_e("not set sys");
      }
      goto _exit;
    }
    
    // log_e("发送给上位机:%s",ser_msg.res_ptr);
    rs485_send(ser_msg->port, ser_msg->res_data, ser_msg->res_size);
  

    if(ser_msg->meta.wrRegQuantity>0 || ser_msg->meta1.quantity>0)
    {
      struct RTU_WRITE rtu_write;
      rt_memset(rtu_write.data,0,sizeof(AGILE_MODBUS_MAX_WRITE_REGISTERS));

      if(ser_msg->meta.wrRegQuantity>0)
      {
        if(ser_msg->meta.function != 16 && ser_msg->meta.function != 23)
        {
          continue;
        }

      // print_asc_frame_meta(&ser_msg.meta);

        for (int i = 0; i < ser_msg->meta.wrRegQuantity; i++) {
          rtu_write.data[i] = ATOHInt(ser_msg->meta.wrBuf + 4 * i);
          // log_e("rtu_write.data[%d]:%02X", i,rtu_write.data[i]);
        }

        int16_t offset = 0; 

        if (g_stConfig.mapEnable) {

          offset = g_stConfig.ascSys[ser_msg->meta.slaveAddr - 1].offset;

          if (ser_msg->meta.wrHead >= 256) {
            offset -= 256;
          }
          // log_d("1wrHead:%d offset:%d,%d", ser_msg.meta.wrHead, offset,
          //       ser_msg.meta.wrHead + offset);
        }

        rtu_write.start_addr = ser_msg->meta.wrHead + offset;
        rtu_write.wrRegQuantity = ser_msg->meta.wrRegQuantity;
      }

      if(ser_msg->meta1.quantity>0)
      {

        if(rt_strcmp(ser_msg->meta1.function,"WWR") == 0 )
        {
          log_i("wrData for chct:%d",ser_msg->meta1.wrData);
          // for (int i = 0; i < ser_msg.meta1.quantity; i++) {
          //   rtu_wr_buf[i] = ATOHInt(ser_msg.meta1.wrData+ 4 * i);
          //   log_d("rtu_wr_buf:%02X", rtu_wr_buf[i]);
          // }
          
          rtu_write.start_addr = ser_msg->meta1.head;
          rtu_write.wrRegQuantity = ser_msg->meta1.quantity;
          rtu_write.data[0] = ser_msg->meta1.wrData;

          // ret = modbus_write_regs(g_ctx, meta1->head,
          //                         meta1->quantity, &meta1->wrData);
          // if (ret != RT_EOK) {
          //   log_e("modbus_write_regs error");
          // }else{
          // }
         rt_memset(ser_msg->meta1.function,'\0',4);
        }
      }
      log_d("send write mq"); 
      ret = rt_mq_send(&rtu_send_mq,&rtu_write,sizeof(struct RTU_WRITE));
      ser_msg->meta.wrByteQuantity = 0;
      ser_msg->meta1.quantity = 0;
    }

_exit:
    rt_memset(ser_msg->req_data,'\0',256);
    rt_memset(ser_msg->res_data,'\0',256);
    rt_mp_free(ser_msg); /* 释放内存块 */
    ser_msg= RT_NULL;

    // rt_mutex_release(dynamic_mutex);
    // start = clock();
    // ascii_parse(&ser_msg);
    // end = clock();
    // log_d("解析完毕,用时:%dms", end - start);
   }
}

// static void thread_asc_resp_entry(void *parameter)
// {
//     rt_uint16_t error_count = 0;
//
//     // struct SER_MSG ser_msg;
//     // rt_memset(ser_msg.data_ptr, '\0', bufsz);
//     // int bufsz = 2048;
//     // ser_msg.data_ptr = rt_malloc(bufsz);
//     struct SER_MSG* ser_msg;
//     struct SER_PORT *resp_port = RT_NULL;
//     clock_t start = 0, end = 0;
//     uint16_t cycles = 0;
//     uint16_t write_len = 0,read_len = 0;
//     char* read_buf=RT_NULL;
//
//     rt_err_t ret;
//     while (1)
//     {
//
//         if(asc_send_mq.entry<1)
//         {
//           rt_thread_mdelay(100);
//           continue;
//         }
//
//         read_len = 0;
//         log_d("asc_send_mq.entry:%d/%d
//         size:%d",asc_send_mq.entry,asc_send_mq.max_msgs,
//               asc_send_mq.msg_size);
//
//        // RT_TICK_PER_SECOND RT_WAITING_FOREVER
//         ret =rt_mq_recv(&asc_send_mq, &ser_msg, sizeof(struct
//         SER_MSG),RT_TICK_PER_SECOND);
//
//         if(ret != RT_EOK)
//         {
//           log_e("rt_mq_recv asc_send_mq faild");
//           if(ret == -RT_ETIMEOUT)
//           {
//             log_e("rt_mq_recv timeout");
//           }else if(ret == -RT_ERROR)
//           {
//             log_e("rt_mq_recv error");
//           }
//           // rt_hw_cpu_reset();
//           continue;
//         }
//
//         read_len = ser_msg->data_size;
//         read_buf = ser_msg->data_ptr;
//         resp_port = ser_msg->port;
//         log_d("asc_send_mq(%d):%s",read_len,read_buf);
//
//         // // 通过邮箱获取到请求的端口,取邮件地址
//         // if (rt_mb_recv(&asc_resp_mb, (rt_ubase_t *)&resp_port, 10) ==
//         RT_EOK) // 10ms
//         // {
//         //     // log_d("%s", resp_port->dev_name);
//         // }
//             // start = clock();
//         write_len = rs485_send(resp_port, read_buf, read_len);
//         if (write_len != read_len)
//         {
//             log_e("发送失败");
//             continue;
//         }
//         log_d("%s->%s",resp_port->dev_name,read_buf);
//         // end = clock();
//         // cycles = end-start;
//         // do
//         // {
//         //     rt_pin_write(LED_RUN, !(rt_pin_read(LED_RUN)));
//         //     rt_thread_mdelay(1);
//         // }while(cycles--);
//
//         // rt_pin_write(LED_RUN, LED_OFF);
//
//         // rt_uint16_t nums = 1;
//         // if (write_len >= 100)
//         //     nums = 1;
//         // else
//         // {
//         //     nums = 2;
//         // }
//         // easyblink(led_run, nums, 50, 100);
//
//         // if (asc_send_mq.entry == 0)
//         // {
//         //   // while (asc_resp_mb.entry > 0)
//         //   // {
//         //   //     log_w("clear asc_resp_mb");
//         //   //
//         //   //     if (rt_mb_recv(&asc_resp_mb, (rt_ubase_t *)&resp_port, 1
//         * RT_TICK_PER_SECOND) == RT_EOK)
//         //   //     {
//         //   //         log_w("resp_port->dev_name:%s clear mailbox",
//         resp_port->dev_name);
//         //   //     }
//         //   // }
//         // }
//
//         // rt_thread_mdelay(1000);// 延时 1s
//     }
// }

// static void thread_rtu_entry(void *parameter)
// {

//     LOG_I("thread_rtu_entry");
//     rt_uint8_t *ptrFrame;
//     rt_uint32_t frame_len = 0;

//     struct SER_MSG ser_msg; /* 用于放置消息的局部变量 */
//     while (1)
//     {
//         if (rt_mq_recv(&rtu_recv_mq, &ser_msg, sizeof(struct SER_MSG),
//         RT_WAITING_FOREVER) == RT_EOK)
//         {
//             // log_d("mq len:%d",ser_msg.data_size);
//             // for (int i = 0; i < ser_msg.data_size; i++)
//             // {
//             //     rt_kprintf("%02X ", *(ser_msg.data_ptr + i));
//             // }
//             // rt_kprintf("\n");

//             // parse_rtu_frame(ser_msg.data_ptr, ser_msg.data_size);
//         }
//     }
// }

#include "agile_modbus.h"

static int cnt = 0;


int threads_init(void) {
  rt_err_t result = RT_EOK;

  // /* 初始化一个 mailbox */
  // result = rt_mb_init(&mb,
  //                     "mbt",               /* 名称是 mbt */
  //                     &mb_pool[0],         /* 邮箱用到的内存池是 mb_pool */
  //                     sizeof(mb_pool) / 4, /*
  //                     邮箱中的邮件数目，因为一封邮件占 4 字节 */
  //                     RT_IPC_FLAG_FIFO);   /* 采用 FIFO 方式进行线程等待 */

  dynamic_mutex = rt_mutex_create("dmutex", RT_IPC_FLAG_PRIO);
  if (dynamic_mutex == RT_NULL)
  {
      rt_kprintf("create dynamic mutex failed.\n");
      return -1;
  }

  // rb = rt_ringbuffer_create(RING_BUFFER_LEN);
  // if (rb == RT_NULL)
  // {
  //     log_e("Can't create ringbffer");
  //     return -1;
  // }

  /* 初始化消息队列
      asc_recv_mq 接收消息队列，接收到的消息都存储到此队列中，也就是生产着
      等待解析线程进行消费
      解析线程消费完了之后，要将消息来源发送到等待消息发送队列 asc_resp_mq 中。
      当 rtu_recv_mq 接收消息队列收到数据后，要消费 asc_resp_mq
     中的消息，并根据消息来源，执行响应。
  */

  result = rt_mq_init(
      &asc_recv_mq, "asc_r", &asc_recv_pool[0], /* 内存池指向 msg_pool */
      ASC_RECV_MSG_SIZE,                        /* 每个消息的大小是 128 字节 */
      sizeof(asc_recv_pool), /* 内存池的大小是 msg_pool 的大小 */
      RT_IPC_FLAG_PRIO);     /* 如果有多个线程等待，优先级大小的方法分配消息*/

  if (result != RT_EOK) {
    log_e("init asc_recv_mq queue failed.\n");
    return -1;
  }

  result = rt_mq_init(
      &rtu_send_mq, "rtu_s", &rtu_send_pool[0], /* 内存池指向 msg_pool */
      RTU_SEND_MSG_SIZE,                        /* 每个消息的大小是 128 字节 */
      sizeof(rtu_send_pool), /* 内存池的大小是 msg_pool 的大小 */
      RT_IPC_FLAG_PRIO);     /* 如果有多个线程等待，优先级大小的方法分配消息*/

  if (result != RT_EOK) {
    log_e("init asc_recv_mq queue failed.\n");
    return -1;
  }

  // result = rt_mq_init(&asc_send_mq,
  //                     "mq_2",
  //                     &asc_send_pool[0],     /* 内存池指向 msg_pool */
  //                     ASC_SEND_MSG_SIZE,    /* 每个消息的大小是 128 字节 */
  //                     sizeof(asc_send_pool), /* 内存池的大小是 msg_pool
  //                     的大小 */ RT_IPC_FLAG_FIFO);     /*
  //                     如果有多个线程等待，优先级大小的方法分配消息*/
  //
  // if (result != RT_EOK)
  // {
  //     log_e("init asc_send_mq queue failed.\n");
  //     return -1;
  // }

  // result = rt_mb_init(&asc_resp_mb,
  //                     "mb",
  //                     &asc_resp_mb_pool[0],         /* 内存池指向 msg_pool */
  //                     sizeof(asc_resp_mb_pool) / 4, /* 每个消息的大小是 128
  //                     字节 */ RT_IPC_FLAG_FIFO);            /*
  //                     如果有多个线程等待，优先级大小的方法分配消息*/
  //
  // if (result != RT_EOK)
  // {
  //     log_e("init asc_resp_mb mailbox failed.\n");
  //     return -1;
  // }

  // result = rt_mq_init(&rtu_recv_mq,
  //                     "mq_3",
  //                     &rtu_recv_pool[0],     /* 内存池指向 msg_pool */
  //                     BYTES_PER_RECV_MSG,         /* 每个消息的大小是 128
  //                     字节 */ sizeof(rtu_recv_pool), /* 内存池的大小是
  //                     msg_pool 的大小 */ RT_IPC_FLAG_FIFO);     /*
  //                     如果有多个线程等待，优先级大小的方法分配消息*/

  // if (result != RT_EOK)
  // {
  //     log_e("init rtu_recv_mq queue failed.\n");
  //     return -1;
  // }


  rt_thread_init(&thread_rtu_master, "rtu_master", thread_rtu_master_entry,
                 RT_NULL, &thread_rtu_master_stack[0],
                 sizeof(thread_rtu_master_stack), THREAD_PRIORITY, THREAD_TIMESLICE);

  rt_thread_startup(&thread_rtu_master);


  rt_thread_init(&thread_asc, "thread_asc", thread_asc_entry, RT_NULL,
                 &thread_asc_stack[0], sizeof(thread_asc_stack),
                 THREAD_PRIORITY, THREAD_TIMESLICE);

  rt_thread_startup(&thread_asc);


  // rt_thread_init(&thread_rtu,
  //                "thread_rtu",
  //                thread_rtu_entry,
  //                RT_NULL,
  //                &thread_rtu_stack[0],
  //                sizeof(thread_rtu_stack), THREAD_PRIORITY,
  //                THREAD_TIMESLICE);

  // rt_thread_startup(&thread_rtu);


  // // 轮询数据线程
  // rt_thread_t thread_mbsend = rt_thread_create("md_m_send",
  // ascii_thread_entry, RT_NULL, 512, MB_POLL_THREAD_PRIORITY, 10); if
  // (thread_mbsend != RT_NULL)
  // {
  //     rt_thread_startup(thread_mbsend);
  // }

  // sprintf(thread_name, "mbp_%s", g_stConfig.serPort[i].dev_name);
  // log_d("This port (%s) is rtu,will as rtu master",
  // g_stConfig.serPort[i].dev_name); rt_thread_t thread_mbpoll =
  // rt_thread_create(thread_name, (void (*)(void
  // *parameter))mbpoll_thread_entry, (void *)i, 1024, MB_POLL_THREAD_PRIORITY,
  // 10); if (thread_mbpoll != RT_NULL)
  // {
  //     rt_thread_startup(thread_mbpoll);
  // }

  return 0;
}

void detach_threads() {}

// void mbpoll_parese_entry(void *parameter)
// {
//     // eMBMasterReqErrCode error_code = MB_MRE_NO_ERR;
//     // USHORT data[2] = {0};
//         // /* Test Modbus Master */
//         // data[0] = (USHORT)(rt_tick_get() / 10);
//         // data[1] = (USHORT)(rt_tick_get() % 10);

//         // error_code =
//         eMBMasterReqWriteMultipleHoldingRegister(g_stConfig.slaveAddr,    /*
//         salve address */
//         // g_stConfig.startRegAddr, /* register start address */
//         // g_stConfig.regsCnt,      /* register total number */
//         //                                                       data, /*
//         data to be written */
//         // RT_WAITING_FOREVER); /* timeout */

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
//     eMBMasterInit(MB_RTU, MB_MASTER_USING_PORT_NUM,
//     MB_MASTER_USING_PORT_BAUDRATE, MB_PAR_NONE); eMBMasterEnable(); while (1)
//     {
//         eMBMasterPoll();
//         rt_thread_mdelay(MB_POLL_CYCLE_MS);
//     }
// }
//
//

// 帧间隔 3.5 * (1+7+1)/9600 
static int read_asc_frame_old(struct SER_PORT *port, char* buf)
{
    int rc =0,len = 0;
    int bufsz = 256;
    char ch;
    while(1)
    {
      if (rt_sem_take(&port->rx_sem, 5) != RT_EOK) {
        break;
      }
      rc = rt_device_read(port->device, -1, &ch, 1);
      if (rc > 0) {
        *(buf+len)=ch;
        len += rc;
        bufsz -= rc;
        if (bufsz <= 0)
          break;
        continue;
      }
      rt_sem_control(&port->rx_sem, RT_IPC_CMD_RESET, RT_NULL);
    }

    if(rtu_send_mq.entry>=0)
    {
      rt_thread_mdelay(10);
    }
    else if(rtu_send_mq.entry>=1)
    {
      rt_thread_mdelay(100 * rtu_send_mq.entry);
    }
  return len;
}

// timeout:-1 == RT_WAITING_FOREVER
static char uart_get_char(struct SER_PORT *port,rt_int32_t timeout)
{
    char ch = 0;
    rt_err_t rc = RT_EOK;
    
    while (rt_device_read(port->device, 0, &ch, 1) == 0)
    {
        rt_sem_control(&port->rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        rc = rt_sem_take(&port->rx_sem, timeout);
        if(rc == -RT_ETIMEOUT)
        {
           break;
        }
    }
    return ch;
}


static int read_asc_frame(struct SER_PORT *port,char* buf)
{
    char i = 0;
    int bufsz = 128; 
    char ch;

    rt_int32_t timeout = RT_TICK_PER_SECOND/100; // 10ms
    rt_int32_t start_time = rt_tick_get();
    
    while(1)
    {
      ch = uart_get_char(port,-1);
      if (ch == '\0') {
        // 检查是否超时
        if (rt_tick_get() - start_time > timeout) {
           break;
        }
        continue;
      }
      if (i >= bufsz - 1) {
          buf[bufsz - 1] = '\0';
          break;
      }
      buf[i++]= ch;
      if (ch == 0x0A && i > 3 && buf[i-2] == 0x0D) {
          buf[i] = '\0';
          break;
      }
    }
    return i;  
}
// :011701050002010A000000D5\r\n01050002010A00000:1212
// 123:011701050002010A000000D5\r\n01050002010A00000:1212
//
rt_uint8_t strip_last(char *buf){
 // 1. 查找第一个 ':'
    char *first_colon = strchr(buf, ':');
    if (first_colon == NULL) {
        return -1;  // 没有找到 ':'
    }

    // 2. 查找 '\r\n'
    char *crlf = strstr(first_colon, "\r\n");
    if (crlf == NULL) {
        return -1;  // 没有找到 '\r\n'
    }

    // 3. 计算要截取的长度（包括 '\r\n'）
    size_t frame_len = crlf - first_colon + 2;  // +2 是因为 "\r\n" 占 2 字节

    // 4. 将数据帧移动到缓冲区开头
    memmove(buf, first_colon, frame_len);

    // 5. 添加字符串结束符
    buf[frame_len] = '\0';
}

void strip_head(char *buf)
{
  // 1. 查找第一个和最后一个 ':'
    char *first_colon = strchr(buf, ':');
    char *last_colon = strrchr(buf, ':');

    // 2. 判断条件：
    //    - 至少有两个 ':'（first_colon != last_colon）
    //    - 或者第一个字符不是 ':'
    if ((first_colon != last_colon) || (buf[0] != ':')) {
        if (last_colon != NULL) {
            strcpy(buf, last_colon);  // 执行复制
        }
    }
    log_d("frame:%s",buf);
}

void strip_tail(char *buf)
{
    // 1. 查找第一个 ':'
    char *first_colon = strchr(buf, ':');
    
    // 2. 查找 '\r\n'
    char *crlf = strstr(buf, "\r\n");
    
    // 3. 判断条件：
    //    - 存在 ':' 和 '\r\n'
    //    - 且 ':' 在 '\r\n' 之前
    if (first_colon != NULL && crlf != NULL && first_colon < crlf) {
        // 计算要截取的长度
        size_t len = crlf - first_colon + 2;
        
        // 复制数据到缓冲区开头
        memmove(buf, first_colon, len);
        
        // 添加字符串结束符
        buf[len] = '\0';
        
    } else {
        // 如果没有找到有效的帧，清空缓冲区
        buf[0] = '\0';
    }

    log_d("frame:%s", buf);
}

// rt_int16_t strip_raw(char *buf) {
//     if (rt_strlen(buf) < 27) {
//         return -1;
//     }
//
//     // 查找最后一个 CRLF
//     char *last_crlf = NULL;
//     char *current = buf;
//     while ((current = strstr(current, "\r\n")) != NULL) {
//         last_crlf = current;
//         current += 2; // 跳过已找到的 CRLF
//     }
//
//     if (last_crlf == NULL) {
//         log_w("No CRLF found");
//         return -1;
//     }
//
//     // 从最后一个 CRLF 向前查找最后一个冒号
//     char *valid_colon = NULL;
//     for (char *p = last_crlf; p >= buf; p--) {
//         if (*p == ':') {
//             valid_colon = p;
//             break;
//         }
//     }
//
//     if (valid_colon == NULL) {
//         log_w("No valid colon found before CRLF");
//         return -1;
//     }
//
//     // 计算帧长度（从冒号到 CRLF + 2）
//     size_t frame_len = last_crlf - valid_colon + 2;
//
//     // 移动数据到缓冲区开头
//     memmove(buf, valid_colon, frame_len);
//
//     // 添加字符串结束符
//     buf[frame_len] = '\0';
//
//     return frame_len;
// }

rt_int16_t strip_raw(char *buf) {
    if (rt_strlen(buf) < 27) {
        return -1;
    }

    // 查找最后一个 CRLF
    char *last_crlf = NULL;
    char *current = buf;
    while ((current = strstr(current, "\r\n")) != NULL) {
        last_crlf = current;
        current += 2; // 跳过已找到的 CRLF
    }

    if (last_crlf == NULL) {
        log_w("No CRLF found");
        return -1;
    }

    // 从最后一个 CRLF 向前查找最后一个冒号
    char *valid_colon = NULL;
    for (char *p = last_crlf; p >= buf; p--) {
        if (*p == ':') {
            valid_colon = p;
            break;
        }
    }

    if (valid_colon == NULL) {
        log_w("No valid colon found before CRLF");
        return -1;
    }

    // 计算帧长度（从冒号到 CRLF + 2）
    size_t frame_len = last_crlf - valid_colon + 2;

    // 确保只有一个 CRLF 结尾
    if (frame_len > 2 && 
        last_crlf > valid_colon + 2 && 
        memcmp(last_crlf - 2, "\r\n", 2) == 0) {
        // 如果倒数第二个也是 CRLF，则只保留一个
        frame_len -= 2;
        last_crlf -= 2;
    }

    // 移动数据到缓冲区开头
    memmove(buf, valid_colon, frame_len);

    // 添加字符串结束符
    buf[frame_len] = '\0';

    return frame_len;
}

int extract_first_frame(char *buf) {
    if (buf == NULL || strlen(buf) < 2) {
        return -1; // 缓冲区无效
    }

    // 1. 查找第一个 ':'
    char *first_colon = strchr(buf, ':');
    if (first_colon == NULL) {
        return -1; // 没有找到 ':'
    }

    // 2. 查找第一个 '\CR\LF'
    char *first_crlf = strstr(first_colon, "\r\n");
    if (first_crlf == NULL) {
        return -1; // 没有找到 '\CR\LF'
    }

    // 3. 计算有效数据长度（从 ':' 到 '\CR\LF' + 2）
    size_t frame_len = first_crlf - first_colon + 2;

    // 4. 移动数据到缓冲区开头
    memmove(buf, first_colon, frame_len);

    // 5. 添加字符串结束符
    buf[frame_len] = '\0';

    return frame_len;
}

rt_bool_t check_whole(char *buf,rt_int16_t len){
  if(len <27)
  {
    return RT_FALSE;
  }

  if(buf[0]!=':')
  {
    return RT_FALSE;
  }

  if(buf[len -1] != 0x0A && buf[len-2] != 0x0D )
  {
    return RT_FALSE;
  }

  return RT_TRUE;

}

void serial_thread_entry(void *parameter) {
  int result;
  int index = (int)parameter;

  struct SER_PORT *port = &g_stConfig.serPorts[index];
  log_d("index:%d %s read thread started %d\n",index, port->dev_name,port->device);


  // log_d("!!!!!!%04x",port->device->flag & RT_DEVICE_FLAG_ACTIVATED);
  // rt_thread_delay(100);
  // log_d("!!!!!!%04x",port->device->flag & RT_DEVICE_FLAG_ACTIVATED);
  

  uart_flush_rx(port->device);

  clock_t start = 0, end = 0;

  int rc=0,len = 0;

  char *buf= port->rx_buf;
  int bufsz = port->config.bufsz;
  rt_memset(buf,'\0',bufsz);

  int stat_req_cnt = 0,total_bytes = 0;

  struct SER_MSG *ser_msg; 

  rt_mp_t tmp_msg_mp = rt_mp_create("temp_mp0", 10, 1024);

  while (1) {
    buf = port->rx_buf;
    rt_memset(buf,'\0',bufsz); 
    len = read_asc_frame_old(port,buf);
    // len = read_asc_frame(port,ser_msg.data_ptr);

    if (len <= 0) {
      rt_thread_mdelay(10);
      continue;
    }

    log_d("len:%d",len);
   
    if(LOG_LVL == LOG_LVL_DBG)
      ulog_hexdump("ASC RCV", 32, buf, len);
    
    // strip_head(buf);
    // strip_tail(buf);
    // strip_last(buf);
    
    len = strip_raw(buf);
    len =  extract_first_frame(buf);
    log_d("buf:%s len:%d",buf,len);
    len = rt_strlen(buf);
    if(!check_whole(buf,len)){
      continue;
    }

    ser_msg= rt_mp_alloc(tmp_msg_mp, RT_WAITING_FOREVER);
    ser_msg->port = port;
    rt_memset(ser_msg->req_data,'\0',256);
    rt_memcpy(ser_msg->req_data,buf,len);
    rt_memset(buf, '\0', port->config.bufsz);

    log_d("ser_msg->req_data:%s",ser_msg->req_data);

    result = rt_mq_send(&asc_recv_mq, &ser_msg, sizeof ser_msg);
    if (result != RT_EOK) {
      if (result == -RT_EFULL) {
        log_e("ASCII 接收线程消息队列已满,请清空");
      } else if (result == -RT_ERROR) {
        log_e("msg maybe too large than max_msgs");
      } else {
        log_e("mq send to asc_resp_mq error unkown");
      }
    }
    ser_msg = NULL;

    port->CanRecv = MAX_BUF_LENGTH;
  }

  if (ser_msg->data_ptr)
    rt_free(ser_msg->data_ptr);
  if (ser_msg->res_ptr)
    rt_free(ser_msg->res_ptr);

}
