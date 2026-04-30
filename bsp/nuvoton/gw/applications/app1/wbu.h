#include <rtdevice.h>

#define RTU_SEND_MQ_POOL 40960

#define READ_HOLDING_CNT 130

extern uint16_t PST_data[READ_HOLDING_CNT];
extern uint8_t UnitType;

int rdp(int argc, char **argv);

extern struct rt_messagequeue rtu_req_mq;
extern struct rt_messagequeue rtu_rsp_mq;
extern rt_uint8_t rtu_req_pool[RTU_SEND_MQ_POOL];
extern rt_uint8_t rtu_rsp_pool[RTU_SEND_MQ_POOL];

extern void SendErrorGetOffStr();

extern void cmd_handler(rt_device_t dev, char *buf, uint8_t len);