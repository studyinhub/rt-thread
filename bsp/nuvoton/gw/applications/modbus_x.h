#ifndef __MODBUS_X_H__
#define __MODBUS_X_H__
#include <string.h>
#include <stdio.h> // sprintf
#include <rtdevice.h>

#include "easyblink.h"
extern ebled_t led_wrk;
extern ebled_t led_run;

#define SER_PORTS_CNT 3

/* 串口接收消息结构*/
struct rx_msg
{
    rt_device_t dev;
    rt_size_t size;
};

struct SER_MSG
{
    rt_uint8_t *data_ptr;  /* 数据块首地址 */
    rt_uint32_t data_size; /* 数据块大小   */
    struct SER_PORT *port; /*来自端口，或者说这条消息要返回给那个 ascii 端口*/
};

extern uint8_t ascii_parse(char *ptrFrame, rt_uint32_t frame_len);

extern uint8_t parse_rtu_frame(char *ptrFrame, rt_uint16_t frame_len);

extern void rtu_master_init(void);
extern int rtu_read_holdings(int regStartAddr, int regCnt, uint16_t *holdBuf);

extern int init_ser_ports();
extern int rs485_send(rt_device_t dev, uint8_t *buf, int len);
extern int rs485_receive(uint8_t *buf, int bufsz, int timeout);
#endif