#include <stdbool.h>
#include "myModbus.h"
#include "myConfig.h"
#include "mySerial.h"

#define LOG_TAG "myModbus"
// #define LOG_LVL LOG_LVL_ERROR // LOG_LVL_INFO
// #define LOG_LVL LOG_LVL_WARNING
// #define LOG_LVL LOG_LVL_INFO
#define LOG_LVL LOG_LVL_DBG // LOG_LVL_DBG LOG_LVL_ERROR
//
#include <ulog.h>

agile_modbus_rtu_t g_ctx_rtu;
agile_modbus_t *g_ctx = &g_ctx_rtu._ctx;

uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
uint8_t ctx_read_buf[SCAN_READ_BYTES]; // AGILE_MODBUS_MAX_ADU_LENGTH

rt_err_t modbus_write_regs(agile_modbus_t *ctx, uint16_t wrHead,
                           uint16_t wrRegQuantity, uint16_t *buf) {
  uint16_t snd_len = 0, rcv_len = 0, rc = 0;
  struct SER_PORT *port = &g_stConfig.serPorts[0];

  rt_int8_t retry_times = 2;
  static rt_int32_t response_timeout = 100, last_rtu_timeout = 100;

  do {

    rt_memset(ctx->send_buf, 0, ctx->send_bufsz);
    rt_memset(ctx->read_buf, 0, ctx->read_bufsz);

    // log_d("wrHead:%d wrRegQuantity:%d", wrHead, wrRegQuantity);

    snd_len = agile_modbus_serialize_write_registers(ctx, wrHead, wrRegQuantity,
                                                     (const uint16_t *)buf);
    if (snd_len >= 512) {
      snd_len = 512;
      return -RT_EOK;
    }
    rs485_send(port, ctx->send_buf, snd_len);
    rcv_len = rs485_receive(port, ctx->read_buf, 8, response_timeout);
    // log_d("rcv_len:%d", rcv_len);
    break;

    if (rcv_len != 8) {
      log_w("retry:%d,%d", retry_times, response_timeout);
      uart_flush_rx(port->device);
      if (response_timeout < 100) {
        response_timeout += 1;
        log_w("response_timeout:%d", response_timeout);
      }
    } else {
      log_d("response_timeout:%d last_rtu_timeout:%d,diff:%d", response_timeout,
            last_rtu_timeout, response_timeout - last_rtu_timeout);
      if (response_timeout - last_rtu_timeout > 10) {
        log_w("update timeout to %d", last_rtu_timeout);
        response_timeout = last_rtu_timeout;
      } else {
        last_rtu_timeout = response_timeout;
      }
      // log_i("response_timeout:%d",response_timeout);
      // log_i("write_regs rs485_receive success(%d,%d)", rcv_len,8);
      break;
    }
  } while (retry_times--);

  if (retry_times <= 0) {
    return -RT_ERROR;
  }

  return RT_EOK;
}

rt_err_t modbus_read_regs(agile_modbus_t *ctx, uint16_t start_addr,
                          uint16_t quantity, uint16_t *buf, uint16_t buf_len) {
  uint16_t snd_len = 0, rcv_len = 0, read_len = 0, rc = 0;
  uint16_t current_addr = start_addr; // 当前读取的起始地址
  uint16_t remaining = quantity;      // 剩余待读取的数量
  uint16_t *current_buf_ptr = buf;    // 当前写入数据的缓冲区指针

  /* 1. 基础参数检查 */
  if (quantity > buf_len) {
    log_e("Read quantity(%d) larger than buf_len(%d).", quantity, buf_len);
    return -RT_ERROR;
  }

  if (quantity == 0) {
    return RT_EOK; // 读取 0 个直接成功
  }

  /* 2. 大循环：处理分包 */
  while (remaining > 0) {
    /* 2.1 计算本次要读取的数量 (单次最大不超过 AGILE_MODBUS_MAX_READ_REGISTERS)
     */
    uint16_t read_once = (remaining > AGILE_MODBUS_MAX_READ_REGISTERS)
                             ? AGILE_MODBUS_MAX_READ_REGISTERS
                             : remaining;
    /* 2.2 计算本次通信需要的缓冲区长度 */
    uint16_t expected_rcv_len =
        read_once * 2 + 5; // 字节数 = 寄存器数*2 + 协议头5字节
    if (expected_rcv_len > ctx->read_bufsz) {
      log_e("Expected len(%d) > read_bufsz(%d)", expected_rcv_len,
            ctx->read_bufsz);
      return -RT_ERROR;
    }
    /* 2.3 动态计算超时时间  */
    rt_int32_t response_timeout = (read_once * 2 + 5) * 5;
    if (response_timeout < 246)
      response_timeout = 246;
    /* 2.4 单包读取的重试循环 */
    rt_int8_t retry_times = 3;
    rt_bool_t read_success = RT_FALSE;
    struct SER_PORT *port = &g_stConfig.serPorts[0];

    do {

      // log_i("Chunk: Addr=%d, Count=%d, Remain=%d", current_addr, read_once,
      //       remaining);
      /* A. 序列化请求 */
      snd_len =
          agile_modbus_serialize_read_registers(ctx, current_addr, read_once);
      if (snd_len != 8) {
        log_w("Serialize error, len=%d", snd_len);
        rt_thread_mdelay(10);
        continue;
      }
      /* B. 发送数据 */
      rs485_send(port, ctx->send_buf, snd_len);
      /* C. 接收数据 */
      rcv_len = rs485_receive(port, ctx->read_buf, expected_rcv_len,
                              response_timeout);

      if (rcv_len <= 0) {
        // log_w("Receive timeout or error (%d)", rcv_len);
        uart_flush_rx(port->device); // 抽干残包
        continue;
      }
      /* D. 校验长度 */
      if (rcv_len != expected_rcv_len) {
        // log_e("Len mismatch: Rcv=%d, Exp=%d", rcv_len, expected_rcv_len);
        uart_flush_rx(port->device);
        // 简单的退避策略
        if (response_timeout < 500)
          response_timeout += 50;
        continue;
      }
      /* E. 反序列化数据 (注意这里传入 current_buf_ptr) */
      int rc = agile_modbus_deserialize_read_registers(ctx, rcv_len,
                                                       current_buf_ptr);
      if (rc < 0) {
        // log_e("Deserialize failed. %d", rc);
        // 如果是异常码，通常不需要重试（比如非法地址），但这里为了鲁棒性还是重试
        continue;
      }
      /* 走到这里说明这一包读取成功 */
      read_success = RT_TRUE;
      break;

    } while (retry_times--);

    /* 2.5 检查单包结果 */
    if (!read_success) {
      log_e("Failed to read chunk after retries.");
      return -RT_ERROR;
    }
    /* 2.6 更新状态，准备下一包 */
    current_addr += read_once;    // 地址偏移
    current_buf_ptr += read_once; // 缓冲区指针偏移
    remaining -= read_once;       // 剩余数量减少
  }
  /* 3. 全部读取完成 */
  rt_memset(ctx->read_buf, 0, ctx->read_bufsz);
  // log_d("Modbus read success: Total %d regs.", quantity);

  return RT_EOK;
}