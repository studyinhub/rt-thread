#include "modbus_x.h"
// #include "myThreads.h"
#include "myConfig.h"
#include "rtdef.h"
#include "rtthread.h"
#include "utils.h"

// #include "mb.h"
// #include "mb_m.h"
// #include "user_mb_app.h"
// #include "crc16.h"

#include <stdint.h>

#define LOG_TAG "modbusx"
#define LOG_LVL LOG_LVL_DBG // LOG_LVL_DBG LOG_LVL_ERROR
// #define LOG_LVL LOG_LVL_ERROR
#include <ulog.h>

int RAddLimitMax = 255; // 允许读寻址的最大值
int RAddLimitMin = 0;   // 允许读寻址的最小值
int WAddLimitMax = 255; // 允许写寻址的最大值
int WAddLimitMin = 0;   // 允许写寻址的最小

// extern struct rt_mailbox asc_resp_mb;
// extern struct rt_messagequeue asc_send_mq;
// extern USHORT usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];

// 01 03 00 01 00 01 D5 CA
// 01 03 00 00 00 0A C5 CD

agile_modbus_rtu_t g_ctx_rtu;
agile_modbus_t *g_ctx = &g_ctx_rtu._ctx;

uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
uint8_t ctx_read_buf[SCAN_READ_BYTES]; // AGILE_MODBUS_MAX_ADU_LENGTH

// 互斥量，当在解析的过程中，禁止更新数据

// void send_rtu_frame(uint8_t slaveAddr, uint8_t funCode, uint16_t regStart,
// uint16_t regCnt, uint16_t bytesCnt, uint8_t *buf)
// {
//     char *pwBuf;
//     int i = 0;
//     uint16_t crc;

//     // log_i("发送功能码%d到%d", funCode, slaveAddr);

//     pwBuf = rt_malloc(24);
//     rt_memset(pwBuf, '\0', 24);
//     struct SER_PORT *port = &g_stConfig.serPorts[0];

//     *pwBuf = slaveAddr;
//     i += 1;

//     *(pwBuf + i) = funCode;
//     i += 1;

//     *(pwBuf + i) = (regStart >> 8) && 0xFF;
//     i += 1;

//     *(pwBuf + i) = regStart & 0x00ff;
//     i += 1;

//     // log_d("读/写寄存器数:(%d)", regCnt);

//     *(pwBuf + i) = (regCnt >> 8) && 0xFF;
//     i += 1;

//     *(pwBuf + i) = regCnt & 0x00FF;
//     i += 1;

//     if (bytesCnt)
//     {
//         // 一个字节
//         //
//         https://img-blog.csdnimg.cn/20190114112803875.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2xpYm94aXU=,size_16,color_FFFFFF,t_70
//         log_d("需要写入字节数:(%d)", bytesCnt);
//         *(pwBuf + i) = bytesCnt;
//         i += 1;
//     }

//     if (buf != NULL)
//     {
//         for (int j = 0; j < bytesCnt; j++)
//         {
//             log_d("i:%d j:%d %02X ", i, j, *(buf + j));
//             *(pwBuf + i + j) = *(buf + j);
//         }
//         i += 2;
//     }

//     crc = CRC(pwBuf, i);        // CRC校验，7+k*2是校验的长度
//     *(pwBuf + i) = crc / 0x100; // 校验值高8位
//     i += 1;

//     *(pwBuf + i) = crc % 0x100; // 校验值低8位
//     i += 1;

//     // log_d("发送到 RTU：crc:%04X", crc);
//     // for (int j = 0; j < i; j++)
//     // {
//     //     rt_kprintf("%02X ", *(pwBuf + j));
//     // }
//     // rt_kprintf("\n");

//     rt_device_write(port->device, 0, pwBuf, i);
//     free(pwBuf);
// }

// :011706000000000000E2

// :01 17 1A 0328 0000 0000 0010 0000 0000 0000 0000 0000 0000 0000 0000 0000
// 93\CR\LF void response_ascii_frame(uint8_t slaveAddr, uint8_t funCode,
// uint8_t *buf, uint16_t bufLen)
// {
//     char *pwBuf;
//     uint8_t calc_lrc = 0;
//     int i = 0;

//     // i = 1;
//     // unsigned char *pointer;
//     // pointer = (unsigned char *)&i;

//     // if (*pointer)
//     // {
//     //     log_w("litttle_endian");
//     // }
//     // else
//     // {
//     //     log_w("big endian/n");
//     // }

//     // for (i = 0; i < byteCnt; i++)
//     // {
//     //     log_d("i:%d v:%02X:", i, *(buf + i));
//     // }

//     // :+（slaveAddr + funcode + quantity bytes + lrc） * 2 + datalen +
//     '\r\n'
//     // 其中 datalen 已经是字节数了

//     // uint16_t frame_len = 1 + (1 + 1 + 1 + 1 + byteCnt) * 2 + 2;
//     // log_d("frame_len:%d", frame_len);

//     // pwBuf = rt_malloc(frame_len);
//     // rt_memset(pwBuf, '\0', frame_len);

//     // *pwBuf = ':';
//     // HToAChar(pwBuf + 1, &slaveAddr, 1);
//     // HToAChar(pwBuf + 3, &funCode, 1);
//     // HToAChar(pwBuf + 5, (uint8_t *)&byteCnt, 1);
//     // HToAChar(pwBuf + 7, (uint8_t *)buf, byteCnt);

//     // // // 补充上 LRC
//     // calc_lrc = ASCII_LRC(pwBuf + 1, 2 * (3 + byteCnt)); //

//     // log_d("calc_lrc:0x%02X", calc_lrc);

//     // HToAChar(pwBuf + 7 + 2 * byteCnt, (uint8_t *)&calc_lrc, 1);
//     // rt_memcpy(pwBuf + 7 + 2 * byteCnt + 2, "\r\n", 2);

//     // // for (int j = 0; j < frame_len; j++)
//     // // {
//     // //     rt_kprintf("%02x ", *(pwBuf + j));
//     // // }

//     // // log_d("res:%s", pwBuf);

//     // for (int i = 0; i < frame_len; i++)
//     // {
//     //     rt_kprintf("%c", *(pwBuf + i));
//     // }

//     // struct SER_PORT *port = &g_stConfig.serPorts[1];
//     // log_d("port:%s", port->dev_name);
//     // rt_device_write(port->device, 0, pwBuf, frame_len);
//     // free(pwBuf);
// }

// void read_regs(uint8_t slaveAddr, uint8_t funCode, uint16_t regStart,
// uint16_t regCnt)
// {
//     switch (funCode)
//     {
//     case 3:
//         // 01 03 00 00 00 0D 00 00 23 04
//         // send_rtu_frame(slaveAddr, funCode, regStart, regCnt, 0, NULL);
//         break;
//     case 1:
//     case 2:
//     case 4:
//         log_e("not suppored");
//         break;
//     default:
//         break;
//     }
// }

// void write_regs(uint8_t slaveAddr, uint8_t funCode, uint16_t regStart,
// uint16_t regCnt, uint16_t bytesCnt, uint8_t *buf)
// {
//     log_d("准备写数据");
//     switch (funCode)
//     {
//     case 5:
//     case 6:
//     case 15:
//         log_e("not suppored");
//         break;
//     case 16:
//         // 01 10 00 0C 00 01 02 00 01 67 5C
//         // 01 10 00 0C 00 01 02 00 00 A6 9C
//         // send_rtu_frame(slaveAddr, funCode, regStart, regCnt, bytesCnt,
//         buf); break;
//     default:
//         break;
//     }
// }

// TODO: error code 02 and 03
//

static uint8_t send_negative(char *pwBuf, uint8_t slaveAddr, uint8_t function,
                             uint8_t errCode) {
  uint8_t calc_lrc = 0;
  uint16_t bufLen = 1 + 8 + 2;

  rt_memset(pwBuf, '\0', bufLen + 1);
  *pwBuf = ':';
  HToAChar(pwBuf + 1, &slaveAddr, 1, 0);
  function += 0x80;
  HToAChar(pwBuf + 3, &function, 1, 0);
  HToAChar(pwBuf + 5, &errCode, 1, 0);

  calc_lrc = ASCII_LRC(pwBuf + 1, bufLen - 5);
  HToAChar(pwBuf + bufLen - 4, (uint8_t *)&calc_lrc, 1, 0);
  *(pwBuf + bufLen - 2) = '\r';
  *(pwBuf + bufLen - 1) = '\n';
  *(pwBuf + bufLen) = '\0';
}

void print_asc_frame_meta(struct ASC_FRAME_META *meta) {
  if (LOG_LVL == LOG_LVL_DBG) {
    log_d("-------------meta----------------");
    log_d("addr_offset:%d", g_stConfig.ascSys[meta->slaveAddr - 1].offset);
    log_d("slaveAddr:%d", meta->slaveAddr);
    log_d("function:%d", meta->function);
    log_d("rdHead:%d", meta->rdHead);
    log_d("rdQuantity:%d", meta->rdQuantity);
    log_d("wrHead:%d", meta->wrHead);
    log_d("wrRegQuantity:%d", meta->wrRegQuantity);
    log_d("wrByteQuantity:%d", meta->wrByteQuantity);
    ulog_hexdump("wrBuf", 16, meta->wrBuf, meta->wrByteQuantity * 2);
    log_d("lrc:%02X(%d),calc_lrc:%02X", meta->lrc, meta->lrc, meta->calc_lrc);
    log_d("--------------------------------------------");
  }
}

rt_err_t ascii_build_response(struct SER_MSG *ser_msg) {

  uint16_t bytesCnt = 0, bufLen = 0;
  uint8_t calc_lrc = 0;

  struct ASC_FRAME_META *meta = &ser_msg->meta;
  char *pwBuf = ser_msg->res_ptr;
  char *prBuf = ser_msg->data_ptr;

  // rt_memcpy(pwBuf, ser_msg->data_ptr, ser_msg->data_size);

  // log_d("1------------pwBuf->%s", pwBuf);

  uint8_t errCode = 0;
  rt_err_t ret = RT_EOK;

  // print_asc_frame_meta(meta);

  // for(uint8_t i;i<meta->wrByteQuantity;i++)
  // {
  //   rt_kprintf("%d ",meta->wrBuf[i]);
  // }

  // rtu_funCode = *(g_ctx->read_buf + 1);
  // log_d("rtu_funCode:%d", rtu_funCode);
  //
  //
  //

  // check slaveaddr if is exist

  uint16_t D90 = g_stConfig.rtuSys.hold[90];
  uint16_t D91 = g_stConfig.rtuSys.hold[91];
  uint16_t D92 = g_stConfig.rtuSys.hold[92];

  // uint16_t DSYS[3] = {D90, D91, D92};

  uint16_t D95 = g_stConfig.rtuSys.hold[95];

  log_d("slaveAddr:%d D90:%d D91:%d D92:%d", meta->slaveAddr, D90, D91, D92);

  switch (D95) {
  case 0:
    if (meta->slaveAddr != D90) {
      return RT_ENOSYS;
    }
    break;
  case 1:
    if (meta->slaveAddr != D90 && meta->slaveAddr != D91) {
      return RT_ENOSYS;
    }
    break;
  case 2:
    if (meta->slaveAddr != D90 && meta->slaveAddr != D91 &&
        meta->slaveAddr != D92) {
      return RT_ENOSYS;
    }
    break;
  }

  ser_msg->res_size = 11;
  if (meta->function != 3 && meta->function != 6 && meta->function != 16 &&
      meta->function != 23) {
    send_negative(pwBuf, meta->slaveAddr, meta->function, 1);
    return RT_EOK;
  }

  if (meta->calc_lrc != meta->lrc) {

    send_negative(pwBuf, meta->slaveAddr, meta->function, 2);
    return RT_EOK;
  }

  if (meta->rdQuantity == 0 && meta->wrRegQuantity == 0) {
    send_negative(pwBuf, meta->slaveAddr, meta->function, 3);
    return RT_EOK;
  }

  log_i("valid frame,to ready response:%d", meta->rdQuantity);

  // ulog_hexdump("test1",16,meta->wrBuf,4);

  rt_memset(pwBuf, '\0', bytesCnt + 1);
  bytesCnt = meta->rdQuantity * 2;
  bufLen = 1 + (1 + 1 + 1 + bytesCnt + 1) * 2 + 2;
  // log_d("ser_msg->data_size:%d bufLen:%d", ser_msg->data_size, bufLen);
  ser_msg->res_size = bufLen;
  *pwBuf = ':';
  HToAChar(pwBuf + 1, &meta->slaveAddr, 1, 0);
  HToAChar(pwBuf + 3, &meta->function, 1, 0);
  HToAChar(pwBuf + 5, (uint8_t *)&bytesCnt, 1, 0);
  if (meta->rdQuantity) {
    // ser_msg->data_ptr = rt_malloc(bufLen + 1);
    // if(pwBuf == RT_NULL)
    //  {
    //    log_e("malloc pwBuf faild");
    //    return -RT_EOK;
    //  }

    int16_t offset = 0;
    if (g_stConfig.mapEnable) {

      offset = g_stConfig.ascSys[meta->slaveAddr - 1].offset;

      log_d("1rdHead:%d offset:%d", meta->rdHead, offset);
      if (meta->rdHead >= 256) {
        offset -= 256;
      }
      log_d("2rdHead:%d offset:%d", meta->rdHead, offset);
    }

    HToAChar(pwBuf + 7,
             (uint8_t *)g_stConfig.rtuSys.hold + 2 * (meta->rdHead + offset),
             bytesCnt, 1);
  }
  // :01 17 00 E8
  // :02 17 00 E7
  calc_lrc = ASCII_LRC(pwBuf + 1, bufLen - 5);
  // log_d("calc_lrc:0x%02X", calc_lrc);
  // 倒数第4个起
  HToAChar(pwBuf + bufLen - 4, (uint8_t *)&calc_lrc, 1, 0);
  // rt_memcpy(pwBuf + bufLen - 2, "\r\n", 2);
  *(pwBuf + bufLen - 2) = '\r';
  *(pwBuf + bufLen - 1) = '\n';
  *(pwBuf + bufLen) = '\0';

  log_d("req:%s", prBuf);
  log_d("2------------pwBuf->%s", pwBuf);

  // log_d("ascii_build_response(%d):", ser_msg->res_size);
  // if (LOG_LVL == LOG_LVL_DBG) {
  //   for (int i = 0; i < ser_msg->data_size; i++) {
  //     rt_kprintf("%c", *(ser_msg->data_ptr + i));
  //   }
  // }

  // log_d("#########################ascii_build_response########################");

  // if(!meta->wrRegQuantity || meta->function == 3)
  // {
  //   return RT_EOK;
  // }

  // log_d("2------------pwBuf->%s",pwBuf);
  // for(uint8_t i;i<meta->wrByteQuantity;i++)
  // {
  //   rt_kprintf("%d ",meta->wrBuf[i]);
  // }

  return RT_EOK;
}

rt_err_t ascii_parse_request(struct SER_MSG *ser_msg) {
  char *ptrFrame = ser_msg->data_ptr;
  rt_uint32_t frame_len = ser_msg->data_size;
  struct ASC_FRAME_META *meta = &ser_msg->meta;

  log_i("pwBuf:%d pResBuf:%d", ptrFrame, ser_msg->res_ptr);

  rt_uint32_t i = 0, j = 0;

  meta->slaveAddr = 0;
  meta->function = 0;

  memset(meta, 0, sizeof(struct ASC_FRAME_META));

  struct frame {
    int frame_start;
    int frame_end;
  };

  struct frame arr[3] = {{-1, -1}};

  char *ch = ptrFrame;

  // do {
  //   rt_kprintf("%c ",*(ch+i));
  //   i++;
  // }while(frame_len--);
  //

  for (i = 0, j = 0; i < frame_len; i++) {
    ch = ptrFrame + i;
    // 查找帧头
    if (*ch == ':' && arr[j].frame_start == -1) {
      arr[j].frame_start = i;
      continue;
    }

    if (*ch == 0x0D) {
      if (*(ch + 1) == 0x0A && arr[j].frame_start != -1 &&
          arr[j].frame_end == -1) {
        arr[j].frame_end = i + 1;
        *(ptrFrame + arr[j].frame_end) = '\0';
        j++;
        // find one
        if (j >= 3)
          break;
      } else {
        continue;
      }
    }
  }

  if (j <= 0) {
    log_e("未找到帧头%d %d", i, frame_len);
    return -RT_ERROR;
  }

  log_d("find %d frame", j);
  // TODO: here just handle one
  for (i = 0; i < 1; i++) {
    char *pStart = ptrFrame + arr[i].frame_start;
    char *pEnd = ptrFrame + arr[i].frame_end;
    log_d("frame_start:%d frame_end:%d frame len:%d", arr[i].frame_start,
          arr[i].frame_end, arr[i].frame_end - arr[i].frame_start + 1);

    meta->slaveAddr = ATOHChar(pStart + 1);
    meta->function = ATOHChar(pStart + 3);

    switch (meta->function) {
    case 3:
      meta->rdHead = ATOHInt(pStart + 5);
      meta->rdQuantity = ATOHInt(pStart + 9);
      meta->lrc = ATOHChar(pStart + 13);
      break;
    case 6:
      meta->wrHead = ATOHInt(pStart + 5);
      meta->wrRegQuantity = 1;
      meta->wrByteQuantity = 2;
      meta->wrBuf = pStart + 9;
      meta->lrc = ATOHChar(pStart + 13);
      break;
    case 16:
      meta->wrHead = ATOHInt(pStart + 5);
      meta->wrRegQuantity = ATOHInt(pStart + 9);
      meta->wrByteQuantity = ATOHInt(pStart + 13);
      meta->wrBuf = pStart + 15;
      meta->lrc = ATOHChar(pStart + 15 + meta->wrByteQuantity * 2);
      break;
    case 23:
      meta->rdHead = ATOHInt(pStart + 5);
      meta->rdQuantity = ATOHInt(pStart + 9);
      meta->wrHead = ATOHInt(pStart + 13);
      meta->wrRegQuantity = ATOHInt(pStart + 17);
      meta->wrByteQuantity = ATOHChar(pStart + 21);
      meta->wrBuf = (pStart + 23);
      meta->lrc = ATOHChar(pStart + 23 + meta->wrByteQuantity * 2);
      break;
    defalut:
      log_w("Unknow funcode");
      return -RT_ERROR;
      break;
    }

    meta->calc_lrc =
        ASCII_LRC(ptrFrame + 1, frame_len - 5); // drop :(1) lrc(2) /r/n(2)
    print_asc_frame_meta(meta);
  }

  // if (asc_slaveAddr != g_stConfig.ascAddr)
  // {
  //     LOG_W("Please check the slave addr");
  //     break;
  // }

  return RT_EOK;
}

int rs485_send(struct SER_PORT *port, uint8_t *buf, int len) {
  rt_size_t write_len = 0;
  RT_ASSERT(port != RT_NULL);
  RT_ASSERT(len > 0);

  rt_device_t dev = port->device;
  // log_d("rs485_send:%s",port->dev_name);

  // struct SER_PORT *port = &g_stConfig.serPorts[0];
  // rt_device_write(port->device, 0, buf, len);
  // ulog_hexdump("rs485_send", 16, buf, len);
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

  /* 从串口读取一个字节的数据，没有读取到则等待接收信号量 */
  int rc = 0;
  // log_d("rs485_receive:%s",port->dev_name);
  rt_sem_take(&port->lock_sem, RT_WAITING_FOREVER);
  // log_d("read bufsz:%d", *bufsz);
  while (1) {

    // rt_sem_control(&port->rx_sem, RT_IPC_CMD_RESET, RT_NULL);
    /* 阻塞等待接收信号量，等到中断后再次读取数据 */
    if (rt_sem_take(&port->rx_sem, rt_tick_from_millisecond(timeout)) !=
        RT_EOK) {
      // log_w("读取超时");
      break;
    }

    rc = rt_device_read(dev, 0, buf + len, bufsz);
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

  rt_sem_release(&port->lock_sem);

  // ulog_hexdump("rs485_receive", 15, buf, len);

  return len;
}

rt_err_t modbus_write_regs(agile_modbus_t *ctx, uint16_t wrHead,
                           uint16_t wrRegQuantity, uint16_t *buf) {
  uint16_t snd_len = 0, rcv_len = 0, rc = 0;
  struct SER_PORT *port = &g_stConfig.serPorts[0];

  uint8_t retry_times = 3;

  do {

    rt_memset(ctx->send_buf, 0, ctx->send_bufsz);
    snd_len = agile_modbus_serialize_write_registers(ctx, wrHead, wrRegQuantity,
                                                     (const uint16_t *)buf);
    rs485_send(port, ctx->send_buf, snd_len);
    log_d("send_len:%d", snd_len);

    if (LOG_LVL == LOG_LVL_DBG)
      ulog_hexdump("send", 16, ctx->send_buf, snd_len);

    rt_memset(ctx->read_buf, 0, ctx->read_bufsz);
    rcv_len = rs485_receive(port, ctx->read_buf, ctx->read_bufsz, 500);
    if (LOG_LVL == LOG_LVL_DBG)
      ulog_hexdump("recv", 16, ctx->read_buf, rcv_len);
    if (rcv_len == 8) {
      log_i("write success");
      break;
    } else {
      log_w("rs485_receive error(%d)", rcv_len);
      log_d("retry:%d \r\n", retry_times);
    }

  } while (retry_times--);

  if (retry_times == 0) {
    return -RT_ERROR;
  }

  return RT_EOK;
}

rt_err_t modbus_read_regs(agile_modbus_t *ctx, uint16_t rdHead,
                          uint16_t rdQuantity, uint16_t *buf) {
  uint16_t snd_len = 0, rcv_len = 0, rc = 0;
  // uint8_t temp[SCAN_READ_BYTES];

  struct SER_PORT *port = &g_stConfig.serPorts[0];
  // rt_device_t dev = g_stConfig.serPorts[0].device;

  snd_len = agile_modbus_serialize_read_registers(ctx, rdHead, rdQuantity);
  // log_d("send_len:%d", snd_len);

  // if(LOG_LVL == LOG_LVL_DBG)
  //   ulog_hexdump("send", 16, ctx->send_buf, snd_len);

  // if (ctx->read_bufsz > 0) {
  //   log_w("drain read_buf");
  //   rt_memset(ctx->read_buf, 0, ctx->read_bufsz);
  // }

  rs485_send(port, ctx->send_buf, snd_len);

  // 读取数据,500 ms 内读完
  // 帧间隔:g_stConfig.serPorts[0].frameInterval

  rcv_len = rs485_receive(port, ctx->read_buf, ctx->read_bufsz, 500);

  if (rcv_len <= 0 || rcv_len != ctx->read_bufsz) {
    log_d("recv:%d bufsz:%d", rcv_len, ctx->read_bufsz);
    log_w("rs485_receive error");
    // log_d("数据读取错误 readsize:%d read_len:%d",g_ctx->read_bufsz,read_len);
    // log_e("没有读取到数据,检查下位机设备是否连接正常");
    // 抽干串口端的数据
    // rs485_receive(temp, SCAN_READ_BYTES, 500);
    // rt_memset(temp, '\0', g_ctx->read_bufsz);
    return -RT_ERROR;
  }

  // if(LOG_LVL == LOG_LVL_DBG)
  //   ulog_hexdump("recv", 16, ctx->read_buf, ctx->read_bufsz);

  rc = agile_modbus_deserialize_read_registers(g_ctx, rcv_len, buf);
  if (rc < 0) {
    log_e("Receive failed.%d", rc);
    if (rc != -1)
      LOG_W("Error code:%d", -128 - rc);
    return -RT_ERROR;
  }

  rt_memset(ctx->read_buf, 0, ctx->read_bufsz);
  // ctx->read_bufsz += rcv_len;

  return RT_EOK;
}

rt_err_t rs232_send_asc() { rt_device_t dev = g_stConfig.serPorts[1].device; }

rt_err_t rs485_send_asc() { rt_device_t dev = g_stConfig.serPorts[2].device; }

void rtu_master_init(void) {
  // 13*16 - 3 =  208
  //
  // agile_modbus_rtu_t ctx_rtu;
  // agile_modbus_t *ctx = &ctx_rtu._ctx;
  agile_modbus_rtu_init(&g_ctx_rtu, ctx_send_buf, sizeof(ctx_send_buf),
                        ctx_read_buf, sizeof(ctx_read_buf));
  agile_modbus_set_slave(g_ctx, g_stConfig.rtuSys.rtuAddr);
}

static rt_err_t uart_tx_com(rt_device_t dev, void *buffer) {
  log_d("%s 发送完毕", dev->parent.name);
}

/* 接收数据回调函数 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size) {

  // log_d("dev:%s\n", dev);
  // log_d("dev.parent.name:%s\n", dev->parent.name);
  // log_d("dev.parent.type:%d\n", dev->parent.type);
  // log_d("dev.parent.flag:%d\n", dev->parent.flag);
  // log_d("dev.device_id:%d\n", dev->device_id);
  // log_d("size:%d\n", size);

  struct SER_PORT *port = NULL;

  /* 串口接收到数据后产生中断，调用此回调函数，然后发送接收信号量 */
  if (size > 0) {
    for (int i = 0; i < SER_PORTS_CNT; i++) {

      port = &g_stConfig.serPorts[i];

      if (!rt_strcmp((char *)port->dev_name, dev->parent.name)) {

        rt_sem_release(&(port->rx_sem));

        // 与 linux 不同，这里不能直接调用 clock(),否则导致系统直接重启
        // 这里要通过信号量通知唤起线程去读取
      }
    }
  }
  return 0;
}

extern void serial_thread_entry(void *parameter);

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
    log_d("default:g_stConfig.serPorts[%d].dev_name %s", i,
          ser_port->dev_name);
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

    rt_kprintf("dev_name:%s\n", ser_port->dev_name);

    ser_port->device = rt_device_find(ser_port->dev_name);

    if (ser_port->device == NULL) {
      rt_kprintf("Can't find %s.\n", ser_port->dev_name);
      return RT_ERROR;
      // goto exit;
    }
    log_d("Find %s device_id:%d", ser_port->dev_name,ser_port->device->device_id);

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
    ret = rt_device_open(ser_port->device, RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(ret == RT_EOK);

    memset(ser_port->rx_buf, 0, MAX_BUF_LENGTH);
    ser_port->CanRecv = MAX_BUF_LENGTH;
    char thread_name[10] = {'\0'};

    sprintf(thread_name, "th_%s", ser_port->dev_name);

    // 对 RTU 口不在再通过线程被动读取，而是采用主动控制的方式
    if (i == 0)
      continue;
    /* 创建 serial 线程 */


    rt_thread_t thread = rt_thread_create(
        thread_name, (void (*)(void *parameter))serial_thread_entry, (void *)i,
        THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
    /* 创建成功则启动线程 */
    if (thread != RT_NULL) {
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
