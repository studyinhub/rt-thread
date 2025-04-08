#ifndef __MODBUS_X_H__
#define __MODBUS_X_H__
#include "agile_modbus.h"
#include "myConfig.h"
#include <rtdevice.h>
#include <stdio.h> // sprintf
#include <string.h>

/* 串口接收消息结构*/
struct UART_RX_MSG {
  rt_device_t dev;
  rt_size_t size;
};

struct RTU_FRAME {
  uint8_t slaveAddr;
  uint8_t funCode;
  uint8_t byteCnt;
  uint8_t resovled;
  // // 本来想在这里定义为 uint16_t 的指针，后来发现反而不合适，需要把16
  // 位数组装起来，然后还要拆开来，否则在转换为 ASCII 码的时候字节顺序不对
  // // 比如 0x1234 会变为 33 34 31 32 也就是 3412
  // uint8_t *pBytes;
  uint16_t crc;
};

#define SCAN_READ_BYTES 205

extern agile_modbus_t *g_ctx;

extern int init_ser_ports();
extern void rtu_master_init(void);

extern int rs485_send(struct SER_PORT *port, uint8_t *buf, int len);
extern int rs485_receive(struct SER_PORT *port, uint8_t *buf, int bufsz,
                         int timeout);

extern rt_err_t parse_serial_frame(struct SER_MSG *ser_msg);
extern rt_err_t ascii_parse(struct SER_MSG *ser_msg);
extern rt_err_t ascii_build_response(struct SER_MSG *ser_msg);
extern rt_err_t rs232_send_asc();
extern rt_err_t rs485_send_asc();
extern rt_err_t rs485_send_rtu();
extern rt_err_t modbus_read_regs(agile_modbus_t *ctx, uint16_t rdHead,
                                 uint16_t rdQuantity, uint16_t *buf);
extern rt_err_t modbus_write_regs(agile_modbus_t *ctx, uint16_t wrHead,
                                  uint16_t wrRegQuantity, uint16_t *buf);
extern void print_asc_frame_meta(struct ASC_FRAME_META *meta);

// extern uint8_t parse_rtu_frame(char *ptrFrame, rt_uint16_t frame_len);

extern int rtu_read_holdings(int regStartAddr, int regCnt, uint16_t *holdBuf);

#endif
