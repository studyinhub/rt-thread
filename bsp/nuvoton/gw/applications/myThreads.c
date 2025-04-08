#include "myThreads.h"
#include "board.h"
#include "modbus_x.h"
#include "myConfig.h"
#include "rtconfig.h"
#include "rtdef.h"
#include "rtthread.h"
#include <rtdevice.h>
#include <sys/types.h>

// #include <stdio.h> // sprintf
// #include <string.h>

#include "utils.h"

#define LOG_TAG "mythread"
// #define LOG_LVL LOG_LVL_DBG // LOG_LVL_INFO
#define LOG_LVL LOG_LVL_ERROR // LOG_LVL_INFO
#include <ulog.h>

// https://blog.csdn.net/lu_embedded/article/details/107308740
// #define RING_BUFFER_LEN 512
// static struct rt_ringbuffer *rb;
// static struct rt_mailbox mb; // 邮箱：用来保存 存储到 ringbufer
// 中的帧长度，当收到 ASCII 帧并送入 ringbuffer
// 后，就发送邮件，内容为此次接收帧的长度 static char mb_pool[128];    /*
// 用于放邮件的内存池 */

// ASCII 接收消息队列
#define ASC_RECV_MSG_SIZE 128
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

struct SER_MSG ser_msg; /* 用于放置消息的局部变量 */

// rt_mutex_t dynamic_mutex = RT_NULL;

static void thread_rtu_master_entry(void *parameter) {

  int i = 0, got_ascAddr = 0, cnt = 0, send_len = 0, read_len = 0, rc = 0;

  rtu_master_init();

  g_stConfig.rtuSys.hold =
      rt_malloc(sizeof(uint16_t) * g_stConfig.rtuSys.scanRegCnt);
  rt_memset(g_stConfig.rtuSys.hold, 0,
            sizeof(uint16_t) * g_stConfig.rtuSys.scanRegCnt);

  struct SER_MSG ser_msg;
  ser_msg.data_size = 0;
  ser_msg.data_ptr = rt_malloc(1024);

  if (ser_msg.data_ptr == RT_NULL) {
    log_e("core mem malloc faild");
  }

  rt_err_t ret;

  uint16_t rtu_wr_buf[AGILE_MODBUS_MAX_WRITE_REGISTERS];

  while (1) {

    // 总扫描开关
    if (!g_stConfig.rtuSys.scanEnable) {
      rt_thread_delay(100);
      continue;
    }

    // if msg queue empty then go await
    if (!rtu_send_mq.entry) {
      
      rt_err_t ret = modbus_read_regs(g_ctx, g_stConfig.rtuSys.scanStAddr,
                                      g_stConfig.rtuSys.scanRegCnt,
                                      g_stConfig.rtuSys.hold);

      rt_thread_delay(g_stConfig.rtuSys.scanInv);
      continue;
    }


    // need to write 
    // rt_memset(g_ctx->read_buf, 0, g_ctx->read_bufsz);
    ret = rt_mq_recv(&rtu_send_mq, &ser_msg, sizeof(struct SER_MSG), 100);

    if (ret != RT_EOK) {
      if (ret == -RT_ETIMEOUT) {
        log_e("rt_mq_recv timeout");
      } else if (ret == -RT_ERROR) {
        log_e("rt_mq_recv error");
      }
      continue;
    }

    // if(LOG_LVL == LOG_LVL_DBG)
    // {
    //   log_d("ser_msg.data_size:%d
    //   port:%s",ser_msg.data_size,ser_msg.port->dev_name);
    //   ulog_hexdump("asc_recv",16,ser_msg.data_ptr,ser_msg.data_size);
    // }

    // log_d("write to rtu %02X",*(ser_msg.meta.wrBuf));
    // ulog_hexdump("wrbuf",16,ser_msg.meta.wrBuf,ser_msg.meta.wrByteQuantity*2);

   
    int16_t offset;
    if(ser_msg.meta.wrByteQuantity>0 && ser_msg.meta.function != 3)
    {
      log_i("wrData for ascii:%d",ser_msg.meta.wrByteQuantity);
      rt_memset(rtu_wr_buf,0,sizeof(rtu_wr_buf));
      for (int i = 0; i < ser_msg.meta.wrRegQuantity; i++) {
        rtu_wr_buf[i] = ATOHInt(ser_msg.meta.wrBuf + 4 * i);
        log_d("rtu_wr_buf:%02X", rtu_wr_buf[i]);
      }

      // print_asc_frame_meta(&ser_msg.meta);
      offset = g_stConfig.ascSys[ser_msg.meta.slaveAddr - 1].offset;

      if (g_stConfig.mapEnable) {

        offset = g_stConfig.ascSys[ser_msg.meta.slaveAddr - 1].offset;

        if (ser_msg.meta.wrHead >= 256) {
          offset -= 256;
        }
        log_d("1wrHead:%d offset:%d,%d", ser_msg.meta.wrHead, offset,
              ser_msg.meta.wrHead + offset);
      }
      //
      ret = modbus_write_regs(g_ctx, ser_msg.meta.wrHead + offset,
                              ser_msg.meta.wrRegQuantity, rtu_wr_buf);
      if (ret != RT_EOK) {
        log_e("modbus_write_regs error");
      }else{
      }
      ser_msg.meta.wrByteQuantity = 0;
    }
    else if(rt_strcmp(ser_msg.meta1.function,"WWR") == 0 )
    {
      log_i("wrData for chct:%d",ser_msg.meta1.wrData);
      offset = 0;
      rt_memset(rtu_wr_buf,0,sizeof(rtu_wr_buf));
      // for (int i = 0; i < ser_msg.meta1.quantity; i++) {
      //   rtu_wr_buf[i] = ATOHInt(ser_msg.meta1.wrData+ 4 * i);
      //   log_d("rtu_wr_buf:%02X", rtu_wr_buf[i]);
      // }
      

      ret = modbus_write_regs(g_ctx, ser_msg.meta1.head+ offset,
                              ser_msg.meta1.quantity, &ser_msg.meta1.wrData);
      if (ret != RT_EOK) {
        log_e("modbus_write_regs error");
      }else{
      }
      rt_memset(ser_msg.meta1.function,'\0',4);
    }


    // rt_mutex_take(dynamic_mutex, RT_WAITING_FOREVER);

    // if (ret != RT_EOK)
    // {
    //     // rt_pin_write(LED_WRK, !(rt_pin_read(LED_WRK)));
    //     easyblink(led_wrk, -1, 500, 500);
    //     continue;
    // }
    //
    // easyblink_stop(led_wrk);
    // eb_led_on(led_wrk);

    // rc = rtu_read_holdings(
    //     g_stConfig.rtuSys.scanStAddr,
    //     g_stConfig.rtuSys.scanRegCnt,
    //     g_stConfig.rtuSys.hold);

    // rt_mutex_release(dynamic_mutex);

    // for (i = 0; i < 3; i++)
    // {
    //     // log_d("i:%d", i, g_stConfig.scanTask[i].enable);
    //     if (g_stConfig.scanTask[i].enable)
    //     {

    //         // log_d(
    //         //     "n:%s,e:%d,a:%d",
    //         //     g_stConfig.scanTask[i].name,
    //         //     g_stConfig.scanTask[i].enable,
    //         //     g_stConfig.scanTask[i].slaveAddr
    //         // );

    //         rc = rtu_read_holdings(
    //             g_stConfig.scanTask[i].slaveAddr,
    //             g_stConfig.scanTask[i].regAddr,
    //             g_stConfig.scanTask[i].cnt,
    //             g_stConfig.scanTask[i].hold);
    //         // ulog_hexdump("dump_hold",16,(uint8_t
    //         *)g_stConfig.scanTask[i].hold,g_stConfig.scanTask[i].cnt * 2);
    //         // for(int j = 0;j<100;j++)
    //         //     log_d("j:%d,v:%d",j,g_stConfig.scanTask[i].hold[j]);

    //         // log_d("asc 地址:%d",g_stConfig.scanTask[i].hold[90+i]);
    //         // 设置 slaveAddr 地址
    //         // g_stConfig.scanTask[i].slaveAddr =
    //         g_stConfig.scanTask[i].hold[90 + i];
    //         // rt_thread_delay(g_stConfig.scanTask[i].inv);
    //     }
    // }

    rt_thread_delay(10);
  }
}

// #define TEST

static void thread_asc_entry(void *parameter) {
  rt_uint16_t error_count = 0;
  LOG_I("thread_asc_entry");

  clock_t start = 0, end = 0;

#ifdef TEST
  struct SER_MSG *ser_msg;
  // struct SER_MSG* ser_msg = (struct SER_MSG*)rt_malloc(sizeof(struct
  // SER_MSG));; ser_msg->data_size = 0; ser_msg->data_ptr = RT_NULL;
#else

  // ser_msg_rsp.data_size = 0;
  // ser_msg_rsp.data_ptr = rt_malloc(27 * 100);
  //
  ser_msg.data_size = 0;
  ser_msg.data_ptr = rt_malloc(27 * 100);
  ser_msg.res_ptr = rt_malloc(27 * 100);

  log_d("data_ptr:%d res_ptr:%d", ser_msg.data_ptr, ser_msg.res_ptr);

  if (ser_msg.data_ptr == RT_NULL || ser_msg.res_ptr == RT_NULL) {
    log_e("core mem malloc faild");
  }

  rt_memset(ser_msg.data_ptr, '\0', 27 * 100);
  rt_memset(ser_msg.res_ptr, '\0', 27 * 100);

#endif
  // log_d("mq buf(%d)",ser_msg->data_size);
  //
  rt_err_t ret;
  while (1) {
    // if msg queue empty then go await
    if (!asc_recv_mq.entry) {
      rt_thread_mdelay(100);
      continue;
    }

    log_d("asc_recv_mq.entry:%d/%d size:%d", asc_recv_mq.entry,
          asc_recv_mq.max_msgs, asc_recv_mq.msg_size);

    log_d("data_ptr:%d res_ptr:%d", ser_msg.data_ptr, ser_msg.res_ptr);

    ret = rt_mq_recv(&asc_recv_mq, &ser_msg, sizeof(struct SER_MSG),
                     RT_WAITING_FOREVER);

    log_i("data_ptr:%d res_ptr:%d", ser_msg.data_ptr, ser_msg.res_ptr);

    if (ret != RT_EOK) {
      if (ret == -RT_ETIMEOUT) {
        log_e("rt_mq_recv timeout");
      } else if (ret == -RT_ERROR) {
        log_e("rt_mq_recv error");
      }
      continue;
    }

    if (LOG_LVL == LOG_LVL_DBG) {
      log_d("ser_msg.data_size:%d port:%s", ser_msg.data_size,
            ser_msg.port->dev_name);
      ulog_hexdump("asc_recv", 16, ser_msg.data_ptr, ser_msg.data_size);
    }

    // if data_size too small then drop
    // :010300000007F5
    // :011700040003000B000204009B000134
    if (ser_msg.data_size < 8) {
      log_d("ser_msg:%d", ser_msg.data_size);
      // if(ser_msg.data_ptr)
      //   rt_free(ser_msg.data_ptr);
      continue;
    }
    // if(ser_msg.data_size != rt_strlen(ser_msg.data_ptr))
    // {
    //     log_e(":%d,%d",ser_msg.data_size,rt_strlen(ser_msg.data_ptr));
    //     continue;
    // }

#ifdef TEST
    log_d("mq buf(%d)", ser_msg->data_size);
#else
    // log_d("mq buf(%d):%s",ser_msg.data_size,ser_msg.data_ptr);

    // log_d("ascii_parse(%d):",ser_msg.data_size);
    if (LOG_LVL == LOG_LVL_DBG) {
      for (int i = 0; i < ser_msg.data_size; i++) {
        rt_kprintf("%c", *(ser_msg.data_ptr + i));
      }
    }

    log_i("data_ptr:%d res_ptr:%d", ser_msg.data_ptr, ser_msg.res_ptr);
    // start = clock();
    //
    ret = parse_serial_frame(&ser_msg);
    // end = clock();
    // log_d("解析完毕,用时:%dms", end - start);
    if (ret != 1 && ret != 2) {
      log_e("parse error,no send");
      continue;
    }

    if(ret == 1)
      ret = ascii_build_response(&ser_msg);
    else{
      ret = chct_build_response(&ser_msg);
    }

    // print_asc_frame_meta(&ser_msg.meta);

    if (ret != RT_EOK) {
      log_e("ascii_build_response error,no send");
      if (ret == RT_ENOSYS) {
        log_e("not set sys");
      }
      continue;
    }

    // rs485_send(ser_msg.port,ser_msg_rsp.data_ptr,ser_msg_rsp.data_size);
    rs485_send(ser_msg.port, ser_msg.res_ptr, ser_msg.res_size);
    // rs485_send(ser_msg.port,ser_msg.res_ptr,ser_msg.data_size);

    // if(!asc_frame_meta.wrRegQuantity || asc_frame_meta.function ==3)
    // {
    //   continue;
    // }

    // ret = rt_mq_send(&rtu_send_mq, &ser_msg_rsp, sizeof(struct SER_MSG));
    ret = rt_mq_send(&rtu_send_mq, &ser_msg, sizeof(struct SER_MSG));

    if (ret != RT_EOK) {
      if (ret == -RT_EFULL) {
        log_e("ASCII 接收线程消息队列已满,请清空");
      } else if (ret == -RT_ERROR) {
        log_e("msg maybe too large than max_msgs");
      } else {
        log_e("mq send to asc_resp_mq error unkown");
      }
      continue;
    }

    // if(ser_msg_rsp.data_ptr)
    // {
    //   rt_free(ser_msg_rsp.data_ptr);
    // }
    // start = clock();
    // ascii_parse(&ser_msg);
    // end = clock();
    // log_d("解析完毕,用时:%dms", end - start);

#endif
  }
#ifdef TEST
  rt_free(ser_msg);
#else
  if (ser_msg.data_ptr)
    rt_free(ser_msg.data_ptr);
#endif
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

  // dynamic_mutex = rt_mutex_create("dmutex", RT_IPC_FLAG_PRIO);
  // if (dynamic_mutex == RT_NULL)
  // {
  //     rt_kprintf("create dynamic mutex failed.\n");
  //     return -1;
  // }

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

  rt_thread_init(&thread_asc, "thread_asc", thread_asc_entry, RT_NULL,
                 &thread_asc_stack[0], sizeof(thread_asc_stack),
                 THREAD_PRIORITY, THREAD_TIMESLICE);

  rt_thread_startup(&thread_asc);

  rt_thread_init(&thread_rtu_master, "rtu_master", thread_rtu_master_entry,
                 RT_NULL, &thread_rtu_master_stack[0],
                 sizeof(thread_rtu_master_stack), 8, THREAD_TIMESLICE);

  rt_thread_startup(&thread_rtu_master);

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

void serial_thread_entry(void *parameter) {
  int result;
  char ch;
  int index = (int)parameter, i = 0, j = 0;

  struct SER_PORT *ser_port = &g_stConfig.serPorts[index];

  log_d("%s read thread started\n", ser_port->dev_name);

  int ReadDatStAdd;  // 上位机要的读的数据起始地
  int ReadDatLenth;  // 上位机要读的数据的长度
  int WriteDatStAdd; // 上位机要写的数据的起始地址
  int WriteDatLenth; // 上位机要写的数据的长度
  int WriteByteNum;  // 上位机要写的数据的字节长度=数据长度*2
  uint8_t slaveAddr; // 上位机请求中的从机地址
  uint8_t funcode;   // 上位机请求中的功能码
  uint8_t lrc;       // 上位机请求中的 LRC

  static int f_index = 0;
  clock_t start = 0, end = 0;
  int readlen = 0;
  int buflen = 0;
  uint8_t calc_lrc = 0;


  char *prBuf = ser_port->rx_buf;
  char *pwBuf = ser_port->tx_buf;

  while (1) {

    // rt_sem_control(&g_stConfig.serPort[index].rx_sem, RT_IPC_CMD_RESET,
    // RT_NULL);
    // rt_sem_take(&g_stConfig.serPort[index].rx_sem, RT_WAITING_FOREVER);
    readlen = rs485_receive(ser_port, prBuf, MAX_BUF_LENGTH, 50);
    //
    // start = clock();
    // // 从第1个字节开始计时，超过 10ms，就终止本次读取
    // while (1)
    // {
    //     if (port->CanRecv <= 1)
    //         port->CanRecv = 1;
    //
    //     // rt_kprintf("CanRecv:%d\n", port->CanRecv);
    //     readlen = rt_device_read(port->device, 0, prBuf + MAX_BUF_LENGTH -
    //     port->CanRecv, port->CanRecv);
    //     // rt_kprintf("readlen:%d\n", readlen);
    //     if (readlen > 0)
    //     {
    //         start = clock(); // 收到数据，重新开始计时，返回值单位：毫秒
    //         port->CanRecv = port->CanRecv - readlen;
    //     }
    //     else
    //     {
    //         end = clock();
    //         rt_thread_delay(1);
    //         // rt_kprintf("end-start:%d\r\n",end-start);
    //         if ((end - start) > g_stConfig.serPort[index].frameInterval)
    //         {
    //             // log_w("%s一帧读取完毕:%d ", port->dev_name, end - start);
    //             break;
    //         }
    //     }
    // }
    //

    // buflen = MAX_BUF_LENGTH - port->CanRecv; // 接收到的总字节数

    if (readlen <= 0) {
      rt_thread_mdelay(10);
      continue;
    }

    log_d("readlen:%d,%d", readlen, ser_port->CanRecv);
    // log_d("buflen:",buflen);

    rt_memcpy(ser_msg.data_ptr, prBuf, readlen);
    // ser_msg.data_ptr = prBuf;
    ser_msg.data_size = readlen;
    ser_msg.port = ser_port;

    if (index >= 1) {
      *(prBuf + readlen) = '\0';
      log_d("ASCII 口收到数据(%d/%d)%s", readlen, ser_port->CanRecv, prBuf);

      // rt_ringbuffer_put(rb, prBuf, buflen);

      /* 发送消息到消息队列中 */
      // result = rt_mq_send(&asc_recv_mq, prBuf, buflen);
      result = rt_mq_send(&asc_recv_mq, &ser_msg, sizeof(struct SER_MSG));

      if (result != RT_EOK) {
        if (result == -RT_EFULL) {
          log_e("ASCII 接收线程消息队列已满,请清空");
        } else if (result == -RT_ERROR) {
          log_e("msg maybe too large than max_msgs");
        } else {
          log_e("mq send to asc_resp_mq error unkown");
        }
        continue;
      }

      /* 发送串口消息到 asc_resp_mq，告诉 RTU 是谁在请求，响应的时候就响应给谁*/
      // log_i("serport index:%s",port->dev_name);
      // log_i("port ddr:0x%08X",port);
      // log_i("port ddr1:0x%08X",&g_stConfig.serPort[index]);
      // if(rt_mb_send(&asc_resp_mb, (struct SER_PORT *)port))

      // 这里还是要发送普通邮件，不能是紧急邮件，如果解析失败，需要将邮件扔掉
      // log_d("ASCII
      // 邮箱数量(%d/%d),port:%s",asc_resp_mb.entry,asc_resp_mb.size,port->dev_name);
      // if (rt_mb_send(&asc_resp_mb, (rt_uint32_t)port))
      // {
      //     log_e("ASCII 接收线程邮件发送失败(%d)\n",asc_resp_mb.size);
      // }

      // 这里加一个延时会降低速度，主要是等待解析线程解析完再去清理 rx_buf
      // 优化方案，可以通过接收解析线程的邮件来处理，达到一个线程间同步的问题
      // rt_thread_delay(1000);
    } else {
      // RTU 口收到数据
      // log_d("RTU 口收到数据:%d", buflen);

      // RTU 收到的数据是 hex 并不是所以不能直接按照字符串来打印
      // ulog_hexdump("rtu_recv",16,prBuf,buflen);
      // for (int j = 0; j < buflen; j++)
      // {
      //     rt_kprintf("%02X ", *(prBuf + j));
      // }
      // rt_kprintf("\n");

      // if (rt_mq_send(&rtu_recv_mq, &ser_msg, sizeof(struct SER_MSG)) !=
      // RT_EOK)
      // {
      //     log_e("rt_mq_send rtu_recv_mq ERR\n");
      // }
    }
    // 不需要清理，直接覆盖即可
    // memset(port->rx_buf, 0, MAX_BUF_LENGTH);
    ser_port->CanRecv = MAX_BUF_LENGTH;
  }
}
