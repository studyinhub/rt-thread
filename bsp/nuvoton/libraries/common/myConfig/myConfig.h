#ifndef __MY_CONFIG_H__
#define __MY_CONFIG_H__

#include <stdint.h>   // uint8_t
#include <drv_uart.h> // serial.h

#include <dfs_posix.h> /* 当需要使用文件操作时，需要包含这个头文件 */

#include "cJSON.h"

#include "tftp.h"

// #define WEBNET_INRAM

#define MAX_BUF_LENGTH 1024

#define MAX_CONFIG_JSON_SIZE 2048
char g_BUF_CONFIG_JSON[MAX_CONFIG_JSON_SIZE];

extern rt_bool_t webnet_in_ram;
extern char g_FmtTimeStr[50];

struct SER_PORT
{
    char dev_name[6];      // uart1 uart6 RT_NAME_MAX
    uint8_t slaveAddr;     // slaveAddr
    uint8_t frameInterval; // 帧间隔，9600 buadrate => 9600 / (2+9+1) < 1
    char prot[6];          // ascii,rtu
    struct serial_configure config;
    struct rt_semaphore rx_sem;  // 该端口接收信号量
    rt_device_t device;          // 串口设备
    int CanRecv;                 // 可以接收的数据
    char rx_buf[MAX_BUF_LENGTH]; // 从上位机接收到的最大的 buf 大小
    char tx_buf[MAX_BUF_LENGTH]; // 发送 buf
};

struct RTU_SYS
{
    uint16_t scanEnable; // rtu 扫读
    uint16_t scanInv;    // 扫读频率
    uint16_t rtuAddr;    // rtu 地址，也就是 plc 地址。
    uint16_t scanStAddr; // 扫读起始地址
    uint16_t scanRegCnt; // 扫读寄存器数量
    uint16_t *hold;
};

struct ASC_SYS
{
    char name[16];
    uint16_t enable;
    uint16_t slaveAddr;
    uint16_t regAddr;
    uint16_t cnt;
    uint16_t offset;
};

struct CONFIG
{
    uint16_t mapEnable;
    struct SER_PORT serPort[3];
    struct RTU_SYS rtuSys;
    struct ASC_SYS ascSys[3];
};

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