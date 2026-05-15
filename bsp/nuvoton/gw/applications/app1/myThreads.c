#include "myConfig.h"
#include "myThreads.h"
#include "myModbus.h"
#include "mySerial.h"
#include "wbu.h"

#define LOG_TAG "mythread"
// #define LOG_LVL LOG_LVL_ERROR // LOG_LVL_INFO
// #define LOG_LVL LOG_LVL_WARNING
#define LOG_LVL LOG_LVL_DBG // LOG_LVL_INFO
#include <ulog.h>

#define RTU_SEND_MSG_SIZE 1024
#define RTU_SEND_MQ_POOL 40960

ALIGN(RT_ALIGN_SIZE)
static char thread_rtu_master_stack[1024];
static struct rt_thread thread_rtu_master;

static void thread_rtu_write_entry(void *parameter) {
  rt_err_t ret;
  struct MB_REQ rtu_write;

  rtu_master_init();

  while (1) {
    // 阻塞等待写入消息（有消息才执行，不占用CPU）
    ret = rt_mq_recv(&rtu_req_mq, &rtu_write, sizeof(struct MB_REQ),
                     RT_WAITING_FOREVER);

    if (ret == RT_EOK) {
      // 执行写入
      ret = modbus_write_regs(g_ctx, rtu_write.start_addr,
                              rtu_write.wrRegQuantity, rtu_write.data);

      if (ret == RT_EOK) {

        PST_data[rtu_write.start_addr] = rtu_write.data[0];

        // 回复消息
        struct MB_RSP rtu_rsp;
        rtu_rsp.addr = rtu_write.start_addr;
        rtu_rsp.value = PST_data[rtu_write.start_addr];
        sprintf(rtu_rsp.msg, "OK");
        rt_mq_send(&rtu_rsp_mq, &rtu_rsp, sizeof(struct MB_RSP));
      }
    }
  }
}

static void thread_rtu_read_entry(void *parameter) {
  while (1) {
    // 周期读取
    modbus_read_regs(g_ctx, 0, READ_HOLDING_CNT, PST_data,
                     ARRAY_SIZE(PST_data));

    UnitType = PST_data[50];
    SendErrorGetOffStr();

    rt_thread_mdelay(200); // 读取延时不影响写入！
  }
}

// static void thread_rtu_master_entry(void *parameter) {

//   int i = 0, got_ascAddr = 0, cnt = 0, send_len = 0, read_len = 0, rc = 0;
//   rt_err_t ret = RT_EOK;

//   rtu_master_init();
//   uart6_dev = rt_device_find("uart6");
//   uart1_dev = rt_device_find("uart1");

//   struct MB_REQ rtu_write;
//   while (1) {
//     if (rtu_req_mq.entry) {
//       ret = rt_mq_recv(&rtu_req_mq, &rtu_write, sizeof(struct MB_REQ), 0);

//       // 内部包含重写
//       ret = modbus_write_regs(g_ctx, rtu_write.start_addr,
//                               rtu_write.wrRegQuantity, rtu_write.data);

//       // 如何重写都失败了，那么就不需要再读取校验并返回了
//       if (ret != RT_EOK) {
//         continue;
//       }
//       PST_data[rtu_write.start_addr] = rtu_write.data[0];

//       struct MB_RSP rtu_rsp;
//       rtu_rsp.addr = rtu_write.start_addr;
//       rtu_rsp.value = PST_data[rtu_write.start_addr];
//       sprintf(rtu_rsp.msg, "%s", "OK");
//       rt_mq_send(&rtu_rsp_mq, &rtu_rsp, sizeof(struct MB_RSP));
//     }

//     modbus_read_regs(g_ctx, 0, READ_HOLDING_CNT, PST_data,
//                      ARRAY_SIZE(PST_data));

//     UnitType = PST_data[50];
//     SendErrorGetOffStr();
//     rt_thread_mdelay(1000);
//   }
// }

void serial_thread_entry(void *parameter) {

  int result;
  int index = (int)parameter;

  struct SER_PORT *port = &g_stConfig.serPorts[index];
  // log_d("index:%d %s read thread started,device_id:%d\n", index,
  // port->dev_name,
  //       port->device->device_id);

  uart_flush_rx(port->device);

  clock_t start = 0, end = 0;

  int rc = 0, len = 0;

  char *buf = port->rx_buf;
  int bufsz = port->config.bufsz;
  rt_memset(buf, '\0', bufsz);

  int stat_req_cnt = 0, total_bytes = 0;

  struct SER_MSG *ser_msg;

  while (1) {

    len = read_asc_frame_old(port, buf);

    // char temp[256];

    // sprintf(temp, "index:%d len:%d\r\n", index, len);

    // uart1_puts(temp);

    // rt_thread_mdelay(1000);

    if (len <= 0) {
      continue;
    }

    cmd_handler(port->device, buf, len);
  }

  if (ser_msg->data_ptr)
    rt_free(ser_msg->data_ptr);
  if (ser_msg->res_ptr)
    rt_free(ser_msg->res_ptr);
}

int threads_init(void) {
  rt_err_t ret = RT_EOK;

  ret = rt_mq_init(
      &rtu_req_mq, "rtu_s", &rtu_req_pool[0], /* 内存池指向 msg_pool */
      RTU_SEND_MSG_SIZE,                      /* 每个消息的大小是 128 字节 */
      sizeof(rtu_req_pool), /* 内存池的大小是 msg_pool 的大小 */
      RT_IPC_FLAG_PRIO);    /* 如果有多个线程等待，优先级大小的方法分配消息*/

  if (ret != RT_EOK) {
    log_e("init rtu_req_pool queue failed.\n");
    return -1;
  }

  ret = rt_mq_init(
      &rtu_rsp_mq, "rtu_r", &rtu_rsp_pool[0], /* 内存池指向 msg_pool */
      RTU_SEND_MSG_SIZE,                      /* 每个消息的大小是 128 字节
                                               */
      sizeof(rtu_req_pool), /* 内存池的大小是 msg_pool 的大小 */
      RT_IPC_FLAG_PRIO);    /*如果有多个线程等待，优先级大小的方法分配消息*/

  if (ret != RT_EOK) {
    log_e("init rtu_rsp_mq queue failed.\n");
    return -1;
  }

  rt_thread_t write_tid, read_tid;

  write_tid = rt_thread_create("rtu_write", thread_rtu_write_entry, RT_NULL,
                               2048, 15, 10);
  read_tid = rt_thread_create("rtu_read", thread_rtu_read_entry, RT_NULL, 2048,
                              15, 10);

  rt_thread_startup(write_tid);
  rt_thread_startup(read_tid);

  // rt_thread_init(&thread_rtu_master, "rtu_master", thread_rtu_master_entry,
  //                RT_NULL, &thread_rtu_master_stack[0],
  //                sizeof(thread_rtu_master_stack), THREAD_PRIORITY,
  //                THREAD_TIMESLICE);

  // rt_thread_startup(&thread_rtu_master);

  return 0;
}