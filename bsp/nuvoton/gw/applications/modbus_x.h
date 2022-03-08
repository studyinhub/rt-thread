#ifndef __MODBUS_X_H__
#define __MODBUS_X_H__
#include <string.h>
#include <stdio.h> // sprintf
#include <rtdevice.h>

#define SER_PORTS_CNT 3


/* 串口接收消息结构*/
struct rx_msg
{
    rt_device_t dev;
    rt_size_t size;
};

extern struct CONFIG g_stConfig;

extern uint8_t parse_ascii_frame(char *ptrFrame, rt_uint32_t frame_len);

extern uint8_t parse_rtu_frame(char *ptrFrame, rt_uint16_t frame_len);

extern int init_ser_ports();
#endif