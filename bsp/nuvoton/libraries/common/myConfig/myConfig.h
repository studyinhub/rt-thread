#ifndef __MY_CONFIG_H__
#define __MY_CONFIG_H__

// #include <drv_uart.h> // serial.h
#include <stdint.h> // uint8_t

#include <dfs_posix.h>

#include "easyblink.h"

#include "cJSON.h"

#include "tftp.h"


#define MAX_BUF_LENGTH 1024

#define MAX_CONFIG_JSON_SIZE 2048
char g_BUF_CONFIG_JSON[MAX_CONFIG_JSON_SIZE];

extern rt_bool_t webnet_in_ram;
extern char g_FmtTimeStr[50];

#define SER_PORTS_CNT 3

#define THREAD_TIMESLICE 5
#define THREAD_PRIORITY 6
#define THREAD_STACK_SIZE 1024

#pragma pack(4)
struct SER_PORT {
  uint8_t device_id;
  char dev_name[7];// uart1 uart6 RT_NAME_MAX
  uint8_t slaveAddr;     // slaveAddr
  uint8_t frameInterval; // 帧间隔，9600 buadrate => 9600 / (2+9+1) < 1
  char prot[6];          // ascii,rtu
  struct serial_configure config;
  struct rt_semaphore rx_sem; // 该端口接收信号量
  struct rt_semaphore lock_sem;
  rt_device_t device;          // 串口设备
  int CanRecv;                 // 可以接收的数据
  char rx_buf[MAX_BUF_LENGTH]; // 从上位机接收到的最大的 buf 大小
  char tx_buf[MAX_BUF_LENGTH]; // 发送 buf
};

struct CHCT_FRAME_META {
  uint8_t sof;// start of frame 
  char slaveAddr[5]; // address of slave "0101" 
  char waittime; // "A"
  char function[4]; // "WRD"
  uint16_t head; // "D05000"
  uint8_t quantity; // "13"
  int16_t wrData; // 00C8
  uint8_t eof[2]; // Not EOF "0x03 0x0D"
};

struct ASC_FRAME_META {
  uint8_t slaveAddr;
  uint8_t function;
  uint16_t rdHead;
  uint16_t rdQuantity;
  uint16_t wrHead;
  uint16_t wrRegQuantity;
  uint8_t wrByteQuantity;
  uint8_t lrc;
  uint8_t calc_lrc;
  char *wrBuf;
};

struct SER_MSG {
  rt_uint8_t *data_ptr; /* 数据块首地址 */
  rt_uint8_t *res_ptr;
  rt_uint32_t data_size; /* 数据块大小   */
  rt_uint32_t res_size;
  struct ASC_FRAME_META meta;
  struct CHCT_FRAME_META meta1;
  struct SER_PORT *port; /*来自端口，或者说这条消息要返回给那个 ascii 端口*/
};
#pragma pack()

struct RTU_SYS {
  uint16_t scanEnable; // rtu 扫读
  uint16_t scanInv;    // 扫读频率
  uint16_t rtuAddr;    // rtu 地址，也就是 plc 地址。
  uint16_t scanStAddr; // 扫读起始地址
  uint16_t scanRegCnt; // 扫读寄存器数量
  uint16_t *hold;
};

struct ASC_SYS {
  char name[16];
  uint16_t enable;
  uint16_t slaveAddr;
  uint16_t regAddr;
  uint16_t cnt;
  uint16_t offset;
};

struct CONFIG {
  uint8_t mapEnable;
  uint8_t transType; // 0:ascii 1:CHCT6302
  struct SER_PORT serPorts[3];
  struct RTU_SYS rtuSys;
  struct ASC_SYS ascSys[3];
};

extern ebled_t led_run;
extern ebled_t led_wrk;

extern char g_BUF_CONFIG_JSON[MAX_CONFIG_JSON_SIZE];

extern cJSON *g_root;

extern struct tftp_server *tftp_server;

extern struct CONFIG g_stConfig;

extern void print_time();
extern void printJSON(cJSON *root);

extern int switch_root(char *path);
extern int save_config(char *path, cJSON *root);
extern int load_config();

#endif
