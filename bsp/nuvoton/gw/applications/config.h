#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h> // uint8_t
#include <drv_uart.h> // serial.h

#include <dfs_posix.h> /* 当需要使用文件操作时，需要包含这个头文件 */

#include "cJSON.h"

#include "tftp.h"


#define MAX_BUF_LENGTH 512

extern rt_bool_t webnet_in_ram;
extern char g_FmtTimeStr[50];

struct SER_PORT
{
    char dev_name[6];  // uart1 uart6 RT_NAME_MAX
    uint8_t slaveAddr; // slaveAddr
    char prot[6];      // ascii,rtu
    struct serial_configure config;
    struct rt_semaphore rx_sem;  // 该端口接收信号量
    rt_device_t device;          // 串口设备
    int CanRecv;                 // 可以接收的数据
    char rx_buf[MAX_BUF_LENGTH]; // 从上位机接收到的最大的 buf 大小
    char tx_buf[MAX_BUF_LENGTH]; // 发送 buf
};

struct CONFIG
{
    uint8_t masterID;
    uint8_t slaveAddr;
    uint16_t startRegAddr;
    uint16_t regsCnt;
    struct SER_PORT serPort[3];
};

extern cJSON *g_root;
extern struct tftp_server *tftp_server;

extern void print_time();
extern void printJSON(cJSON *root);

extern int switch_root(char* path);
extern int save_config(char *path, cJSON *root);
extern int load_config();

#endif