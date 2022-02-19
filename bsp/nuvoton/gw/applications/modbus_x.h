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


extern int init_ser_ports();
#endif