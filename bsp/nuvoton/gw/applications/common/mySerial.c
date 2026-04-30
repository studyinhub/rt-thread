
#include "mySerial.h"

extern void serial_thread_entry(void *parameter);

#define LOG_TAG "mySerial"
// #define LOG_LVL LOG_LVL_ERROR // LOG_LVL_INFO
// #define LOG_LVL LOG_LVL_WARNING
#define LOG_LVL LOG_LVL_INFO
// #define LOG_LVL LOG_LVL_DBG // LOG_LVL_DBG LOG_LVL_ERROR
//
#include <ulog.h>

#define DEBUG_PORT 2

static rt_err_t uart_tx_com(rt_device_t dev, void *buffer) {
  log_d("%s 发送完毕", dev->parent.name);
}

void uart_flush_rx(struct rt_device *dev) {
  // struct rt_device *dev;
  uint8_t buf[300];
  rt_size_t len;

  // dev = rt_device_find(uart_name);
  // log_d("flush:%d",dev);
  // if (dev == RT_NULL)
  // {
  //     rt_kprintf("UART device %s not found!\n", uart_name);
  //     return;
  // }

  while (1) {
    len = rt_device_read(dev, 0, buf, sizeof(buf));
    if (len > 0) {
      // 接收到数据，但丢弃
    } else if (len == 0) {
      // 没有数据，缓冲区已空
      break;
    } else {
      rt_kprintf("Error flushing UART %d\n", len);
      break;
    }
  }

  // rt_kprintf("UART[%s] RX buffer flushed.\n", uart_name);
}

int rs485_send(struct SER_PORT *port, uint8_t *buf, int len) {
  rt_size_t write_len = 0;
  // RT_ASSERT(port != RT_NULL);
  // RT_ASSERT(len > 0);

  // log_d("port:%s len:%d", port->dev_name, len);

  if (len > 512) {
    len = 512;
    log_w("len > 512");
  }

  rt_device_t dev = port->device;

  // struct SER_PORT *port = &g_stConfig.serPorts[0];
  // rt_device_write(port->device, 0, buf, len);
  // 0 : RTU,2:ASCII 485 1: ASC 232
  // if(LOG_LVL == LOG_LVL_DBG)
  if (LOG_LVL == LOG_LVL_DBG && port->device_id == DEBUG_PORT) {
    log_d("rs485_send:%s:%d", port->dev_name, len);
    if (len > 0 && len < 300) {
      log_d("resp->%s", buf);
      ulog_hexdump("rs485_send", 16, buf, len);
    } else {
      log_e("send_error:%d", len);
    }
  }
  rt_sem_take(&port->lock_sem, RT_WAITING_FOREVER);
  write_len = rt_device_write(dev, 0, buf, len);
  // if(rt_strcmp(port->dev_name,"uart8")!=0)
  //    log_d("write_len:%d,%d",write_len,len);
  if (write_len != len) {
    log_e("not all data wrote");
  }
  rt_sem_release(&port->lock_sem);

  // if (!rt_strcmp((char *)dev->parent.name, "uart6"))
  // {
  //  log_e("%s 发送完毕",dev->parent.name);
  // }
  return len;
}

// timeout:要求下位机必须在 timeout 时间内回复，否则会认为读取超时
int rs485_receive(struct SER_PORT *port, uint8_t *buf, int bufsz, int timeout) {
  int len = 0;
  // struct SER_PORT *port = &g_stConfig.serPort[0];
  rt_device_t dev = port->device;

  rt_tick_t start_tick = rt_tick_get();
  rt_tick_t timeout_tick = rt_tick_from_millisecond(timeout);

  // if (bufsz > 0) {
  //   log_w("drain read_buf");
  //   rt_memset(buf, 0, bufsz);
  // }

  /* 从串口读取一个字节的数据，没有读取到则等待接收信号量 */
  int rc = 0;
  // log_d("rs485_receive:%s",port->dev_name);
  // rt_sem_take(&port->lock_sem, RT_WAITING_FOREVER);
  // log_d("read bufsz:%d", *bufsz);
  while (1) {

    if (rt_tick_get() - start_tick >= timeout_tick) {
      break; // 整体超时
    }

    // rt_sem_control(&port->rx_sem, RT_IPC_CMD_RESET, RT_NULL);
    /* 阻塞等待接收信号量，等到中断后再次读取数据 10ms */
    if (rt_sem_take(&port->rx_sem, RT_TICK_PER_SECOND / 100) == RT_EOK) {
      // log_w("读取超时%d",timeout);
      rc = rt_device_read(dev, -1, buf + len, 1);
      if (rc > 0) {
        // if (rt_strcmp(port->dev_name, "uart6") == 0) {
        //   rt_kprintf("rc:%d,bufsz:%d", rc, bufsz);
        // }
        len += rc;
        bufsz -= rc;
        if (bufsz <= 0)
          break;
        continue;
      }
    }
  }

  // if(len>0 && LOG_LVL == LOG_LVL_DBG)
  if (len > 0 && port->device_id == DEBUG_PORT && LOG_LVL == LOG_LVL_DBG)
    ulog_hexdump("rs_485 recv", 32, buf, len);

  // uart_flush_rx(dev);

  rt_sem_release(&port->lock_sem);

  return len;
}

static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size) {

  // log_d("dev:%s\n", dev);
  // log_d("dev.parent.name:%s\n", dev->parent.name);
  // log_d("dev.parent.type:%d\n", dev->parent.type);
  // log_d("dev.parent.flag:%d\n", dev->parent.flag);
  // log_d("dev.device_id:%d\n", dev->device_id);
  // log_d("size:%d\n", size);

  struct SER_PORT *port = NULL;
  // if(size <=0) return 0;

  for (int i = 0; i < SER_PORTS_CNT; i++) {
    // rt_kprintf("port->device_id:%d
    // dev->device_id:%d\r\n",port->device_id,dev->device_id); if
    // (!rt_strcmp((char *)port->dev_name, dev->parent.name)) {
    if (g_stConfig.serPorts[i].device_id == dev->device_id) {
      port = &g_stConfig.serPorts[i];
      break;
    }
  }

  if (port != NULL) {
    rt_sem_release(&(port->rx_sem));
  }

  return 0;
}

int init_ser_ports() {

  rt_err_t ret = RT_EOK;
  struct serial_configure temp_config = RT_SERIAL_CONFIG_DEFAULT;
  cJSON *ports, *port, *scan, *tempObj;
  int iArrayCnt = 0;
  ports = cJSON_GetObjectItem(g_root, "serPorts");
  iArrayCnt = cJSON_GetArraySize(ports);
  log_d("iArrayCnt:%d", iArrayCnt);

  scan = cJSON_GetObjectItem(g_root, "scan");

  // bug: 这里获取不到 bool 的正真值，永远返回的都是 0！！！！！
  tempObj = cJSON_GetObjectItem(scan, "test");
  log_d("%s %d", tempObj->string, tempObj->valueint);
  tempObj = cJSON_GetObjectItem(scan, "test1");
  log_d("%s %d", tempObj->string, tempObj->valueint);

  // log_d("g_stConfig.scanEnable:%d", g_stConfig.scanEnable);
  // tempObj = cJSON_GetObjectItem(scan, "scanEnable");
  // if (NULL == tempObj)
  // {
  //     log_e("get item faild!");
  // }
  // g_stConfig.scanEnable = tempObj->valueint;
  // log_d("g_stConfig.scanEnable:%s %d,%d", tempObj->string,
  // g_stConfig.scanEnable, tempObj->valueint);

  // tempObj = cJSON_GetObjectItem(scan, "scanInv");
  // g_stConfig.scanInv = tempObj->valueint;
  // log_d("g_stConfig.scanInv:%d", g_stConfig.scanInv);

  // tempObj = cJSON_GetObjectItem(scan, "scanStAddr");
  // g_stConfig.scanStAddr = tempObj->valueint;

  // tempObj = cJSON_GetObjectItem(scan, "scanRegCnt");
  // g_stConfig.scanRegCnt = tempObj->valueint;
  for (int i = 0; i < iArrayCnt; i++) {

    struct SER_PORT *ser_port = &g_stConfig.serPorts[i];

    // 先赋值为默认值
    ser_port->config = temp_config;

    port = cJSON_GetArrayItem(ports, i);
    tempObj = cJSON_GetObjectItem(port, "name");
    log_d("default:g_stConfig.serPorts[%d].dev_name %s", i, ser_port->dev_name);
    log_d("---get name:%s type:%d value:%s", tempObj->string, tempObj->type,
          tempObj->valuestring);

    rt_memset(ser_port->dev_name, '\0', 6);
    rt_strncpy(ser_port->dev_name, tempObj->valuestring,
               rt_strlen(tempObj->valuestring));
    log_d("new:g_stConfig.serPorts[%d].dev_name %s", i, ser_port->dev_name);

    // 设置波特率
    tempObj = cJSON_GetObjectItem(port, "baudrate");
    log_d("default:g_stConfig.serPorts[%d].config.baud_rate %d", i,
          ser_port->config.baud_rate);
    log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type,
          tempObj->valueint);
    ser_port->config.baud_rate = tempObj->valueint;
    log_d("new:g_stConfig.serPorts[%d].config.baud_rate %d", i,
          ser_port->config.baud_rate);

    // 依据波特率，设置帧间隔
    // g_stConfig.serPorts[i].frameInterval = 10000 /
    // g_stConfig.serPorts[i].config.baud_rate + 2 + 2;
    ser_port->frameInterval = 10;
    log_d("new:g_stConfig.serPorts[%d].frameInterval %d", i,
          ser_port->frameInterval);

    // 设置数据位
    tempObj = cJSON_GetObjectItem(port, "databits");
    log_d("default:g_stConfig.serPorts[%d].config.data_bits %d", i,
          ser_port->config.data_bits);
    log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type,
          tempObj->valueint);
    g_stConfig.serPorts[i].config.data_bits = tempObj->valueint;
    log_d("new:g_stConfig.serPorts[%d].config.data_bits %d", i,
          ser_port->config.data_bits);

    // 设置停止位
    tempObj = cJSON_GetObjectItem(port, "stopbits");
    log_d("default:g_stConfig.serPorts[%d].config.stop_bits %d", i,
          ser_port->config.stop_bits);
    log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type,
          tempObj->valueint);
    ser_port->config.stop_bits = tempObj->valueint;
    log_d("new:g_stConfig.serPorts[%d].config.stop_bits %d", i,
          ser_port->config.stop_bits);

    // 设置奇偶校验
    tempObj = cJSON_GetObjectItem(port, "parity");
    log_d("default:g_stConfig.serPorts[%d].config.parity %d", i,
          ser_port->config.parity);
    log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type,
          tempObj->valueint);
    ser_port->config.parity = tempObj->valueint;
    log_d("new:g_stConfig.serPorts[%d].config.parity %d", i,
          ser_port->config.parity);

    // 设置 bufsz
    tempObj = cJSON_GetObjectItem(port, "bufsz");
    log_d("default:g_stConfig.serPorts[%d].config.bufsz %d", i,
          ser_port->config.bufsz);
    log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type,
          tempObj->valueint);
    ser_port->config.bufsz = tempObj->valueint;
    log_d("new:g_stConfig.serPorts[%d].config.bufsz %d", i,
          ser_port->config.bufsz);

    ser_port->device = rt_device_find(ser_port->dev_name);

    if (ser_port->device == NULL) {
      rt_kprintf("Can't find %s.\n", ser_port->dev_name);
      return RT_ERROR;
      // goto exit;
    }
    log_d("Find %s device_id:%d", ser_port->dev_name, ser_port->device_id);
    ser_port->device->device_id = ser_port->device_id;

    // https://www.rt-thread.org/document/site/#/rt-thread-version/rt-thread-standard/programming-manual/device/uart/uart_v1/uart

    rt_device_control(ser_port->device, RT_DEVICE_CTRL_CONFIG,
                      &ser_port->config);

    /* 发送字符串 */
    // char str[]="hello";
    // rt_device_write(serial, 0, str, (sizeof(str) - 1));

    /* 设置接收回调函数 */
    rt_device_set_rx_indicate(ser_port->device, uart_rx_ind);

    /* 设置发送完成回调函数 没有作用，驱动不支持*/
    rt_device_set_tx_complete(ser_port->device, uart_tx_com);

    /* 没有作用，驱动不支持*/
    // rt_device_control(ser_port->device,
    // RT_DEVICE_CTRL_SET_INT,RT_DEVICE_FLAG_INT_TX);

    /* 初始化信号量 */
    char sem_name[10] = {'\0'};
    sprintf(sem_name, "%s_rx", ser_port->dev_name);
    rt_sem_init(&ser_port->rx_sem, sem_name, 0, RT_IPC_FLAG_PRIO);
    sprintf(sem_name, "%s_lock", ser_port->dev_name);
    rt_sem_init(&ser_port->lock_sem, sem_name, 1, RT_IPC_FLAG_PRIO);

    /* Interrupt RX */
    ret = rt_device_open(ser_port->device,
                         RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(ret == RT_EOK);

    memset(ser_port->rx_buf, 0, MAX_BUF_LENGTH);
    ser_port->CanRecv = MAX_BUF_LENGTH;
    char thread_name[10] = {'\0'};

    sprintf(thread_name, "th_%s", ser_port->dev_name);

    // 对 RTU 口不在再通过线程被动读取，而是采用主动控制的方式
    if (i == 0)
      continue;
    /* 创建 serial 线程 */

    // if (ser_port->device_id == 1) {
    //   continue;
    // }

    rt_thread_t thread = rt_thread_create(
        thread_name, (void (*)(void *parameter))serial_thread_entry, (void *)i,
        THREAD_STACK_SIZE, THREAD_PRIORITY - 1, THREAD_TIMESLICE);
    /* 创建成功则启动线程 */
    if (thread != RT_NULL) {
      log_d("start %s", thread_name);
      rt_thread_startup(thread);
    } else {
      ret = RT_ERROR;
      ret = rt_device_close(ser_port->device);
      goto exit;
    };
  }

  return 0;

exit:
  RT_ASSERT(ret == RT_EOK);
  return ret;
}

int read_asc_frame_old(struct SER_PORT *port, char *buf) {
  int rc = 0, len = 0;
  int bufsz = 256;
  char ch;
  while (1) {
    if (rt_sem_take(&port->rx_sem, RT_TICK_PER_SECOND / 10) != RT_EOK) {
      break;
    }
    rc = rt_device_read(port->device, -1, &ch, 1);
    if (rc > 0) {
      *(buf + len) = ch;
      len += rc;
      bufsz -= rc;
      if (bufsz <= 0)
        break;
      continue;
    }
    // rt_sem_control(&port->rx_sem, RT_IPC_CMD_RESET, RT_NULL);
  }

  // if (rtu_send_mq.entry >= 0) {
  //   rt_thread_mdelay(10);
  // } else if (rtu_send_mq.entry >= 1) {
  //   rt_thread_mdelay(100 * rtu_send_mq.entry);
  // }
  return len;
}

rt_device_t uart6_dev = RT_NULL;
rt_device_t uart1_dev = RT_NULL;

static void uart_send(rt_device_t dev, const char *buf, int len) {
  if (dev != RT_NULL)
    rt_device_write(dev, 0, buf, len);
}

void uart1_puts(char *str) { uart_send(uart1_dev, str, strlen(str)); }

void uart6_puts(char *str) { uart_send(uart6_dev, str, strlen(str)); }