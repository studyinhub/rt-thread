
#include <rtdevice.h>
#include <stdio.h> // sprintf
#include <string.h>
#include "rtdef.h"

#include "myConfig.h"

extern rt_device_t uart1_dev;
extern rt_device_t uart6_dev;

extern void uart1_puts(char *str);
extern void uart6_puts(char *str);

extern void uart_flush_rx(struct rt_device *dev);

extern int rs485_send(struct SER_PORT *port, uint8_t *buf, int len);
extern int rs485_receive(struct SER_PORT *port, uint8_t *buf, int bufsz,
                         int timeout);
extern int init_ser_ports();

extern int read_asc_frame_old(struct SER_PORT *port, char *buf);