
#include <string.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <stdbool.h>

#include "mySerial.h"

#include "myModbus.h"
#include "myUtils.h"

#define LOG_TAG "wbu"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>

#include "wbu.h"

uint8_t UnitType = 2;

struct rt_messagequeue rtu_req_mq;
rt_uint8_t rtu_req_pool[RTU_SEND_MQ_POOL];
struct rt_messagequeue rtu_rsp_mq;
rt_uint8_t rtu_rsp_pool[RTU_SEND_MQ_POOL];

uint16_t PST_data[130] = {0};

bool check_lrc(char *cmd, uint8_t lrc) {
  char temp[128] = {0};
  sprintf(temp, "%s ", cmd);
  uint8_t calc_lrc = calculate_lrc((uint8_t *)temp, strlen(temp));
  return lrc == calc_lrc ? true : false;
}

// Ver固定字符串
static uint8_t ver_lable[13][120] = {"MED MR  (c)   AIRSYS "
                                     "\r\n====================================="
                                     "===============\r\n\r\n          Appl: V",
                                     ".",
                                     "- ",
                                     ".",
                                     ".",
                                     "\r\n          Boot: ",
                                     ".",
                                     ".",
                                     "\r\n          HW  : Rev",
                                     ".",
                                     ".",
                                     "\r\n          SNr : ",
                                     "\r\n          Type :"};

static const uint8_t CharTable[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

char pVerChr[512] = {
    "MED MR  (c)   AIRSYS "
    "\r\n====================================================\r\n\r\n          "
    "Appl: V0.0- 03.02.0101 \r\n          Boot:00104 \r\n          HW  : Rev2 "
    "\r\n          SNr :00106\r\n   Type: CCS"};

void Dec_ASCII_BCD(uint16_t Dec, uint8_t lenfixed, uint8_t DecValASCII[8]) {
  uint8_t temp4[7] = {0};
  uint16_t temp2; // 16位无符号整型数
  uint16_t temp3; // 16位无符号整型数
  int len = 0;
  int k = 0; // 位权
  int len2 = 0;
  int k2 = 0;
  temp3 = Dec;
  temp2 = Dec;
  rt_memset(DecValASCII, '\0', 8);
  if (temp3 > 32766) {
    len = 1;
    temp2 = 65536 - temp2;
    DecValASCII[0] = '-';
    while (temp2) // 数值被取完
    {
      temp4[k] = temp2 % 10; // 取余数
      temp2 = temp2 / 10;    // 十进制向右移一位
      k++;
    }
    k2 = k;
    while (lenfixed - k2 > 0 && lenfixed > 0 && lenfixed < 6) {
      DecValASCII[len++] = '0';
      k2++;
    }
    for (len2 = 0; len2 < k; len2++) {
      DecValASCII[len + len2] = CharTable[temp4[k - len2 - 1]];
      ; // 将字符串放进发送数组中
    }
    DecValASCII[len + len2] = '\0';
  } else if (temp3 > 0) {
    len = 0;
    while (temp2) // 数值被取完
    {
      temp4[k] = temp2 % 10; // 取余数
      temp2 = temp2 / 10;    // 十进制向右移一位
      k++;
    }
    k2 = k;
    while (lenfixed - k2 > 0 && lenfixed > 0 && lenfixed < 6) {
      DecValASCII[len++] = '0';
      k2++;
    }
    for (len2 = 0; len2 < k; len2++) {
      DecValASCII[len + len2] =
          CharTable[temp4[k - len2 - 1]]; // 将字符串放进发送数组中
    }
    DecValASCII[len + len2] = '\0';
  } else if (temp3 == 0) {
    len = 0;
    if (lenfixed == 0) {
      DecValASCII[0] = '0';
      DecValASCII[1] = '\0';
    } else if (lenfixed < 6) {
      while (lenfixed - len > 0 && lenfixed > 0 && lenfixed < 6) {
        DecValASCII[len++] = '0';
      }
      DecValASCII[len] = '\0';
    }
  }
}

uint8_t ver_TXBuffer[512] = {0}; // ver回复字符串。用于发送

void VerHandleWBU() // WBU机组Ver字符串处理函数(版本查询回复字符串处理函数)
{
  uint8_t DecValASCII[8];
  rt_memset(pVerChr, '\0', sizeof(pVerChr));
  strcpy(
      (char *)pVerChr,
      (char const *)ver_lable
          [0]); //"MED MR  (c)   AIRSYS
                //\r\n====================================================\r\n\r\n
                // Appl: V",
  Dec_ASCII_BCD(PST_data[51] / 100, 0, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); // 大版本号

  strcat((char *)pVerChr, (char const *)ver_lable[1]); //"."

  Dec_ASCII_BCD(PST_data[51] % 100, 2, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); // 小版本号

  strcat((char *)pVerChr, (char const *)ver_lable[2]); //" - "

  Dec_ASCII_BCD(PST_data[52], 2, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); // 日

  strcat((char *)pVerChr, (char const *)ver_lable[3]); //"."

  Dec_ASCII_BCD(PST_data[53], 2, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); // 月

  strcat((char *)pVerChr, (char const *)ver_lable[4]); //"."
  if (PST_data[54] <= 99) {
    Dec_ASCII_BCD(PST_data[54] + 2000, 4, DecValASCII);
  } else {
    Dec_ASCII_BCD(PST_data[54], 4, DecValASCII);
  }

  strcat((char *)pVerChr, (char const *)DecValASCII); // 年

  strcat((char *)pVerChr, (char const *)ver_lable[5]); //"          Boot:"

  Dec_ASCII_BCD(PST_data[55], 0, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); //

  strcat((char *)pVerChr, (char const *)ver_lable[6]); //"."

  Dec_ASCII_BCD(PST_data[56], 0, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); //

  strcat((char *)pVerChr, (char const *)ver_lable[7]); //"."

  Dec_ASCII_BCD(PST_data[57], 3, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); //

  strcat((char *)pVerChr, (char const *)ver_lable[8]); //"          HW  : Rev"
  Dec_ASCII_BCD(PST_data[55], 0, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); //

  strcat((char *)pVerChr, (char const *)ver_lable[9]); //"."
  Dec_ASCII_BCD(PST_data[56], 0, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); //

  strcat((char *)pVerChr, (char const *)ver_lable[10]); //"."
  Dec_ASCII_BCD(PST_data[57], 3, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); //

  strcat((char *)pVerChr, (char const *)ver_lable[11]); //"          SNr : "
  Dec_ASCII_BCD(PST_data[58], 0, DecValASCII);
  strcat((char *)pVerChr, (char const *)DecValASCII); // SNr

  strcat((char *)pVerChr, (char const *)ver_lable[12]); //"          Type : "
  strcat((char *)pVerChr, "WBU");                       // "          WBU: "

  // strcat((char *)pVerChr, "\r\n"); // 回车换行

  rt_memset(ver_TXBuffer, '\0', sizeof(ver_TXBuffer));
  strcpy((char *)ver_TXBuffer, (char const *)pVerChr);
}

int ver(int argc, char **argv) {

  VerHandleWBU();
  rt_kprintf("%s", ver_TXBuffer);
  return 0;
}

MSH_CMD_EXPORT(ver, Get WBU version);
MSH_CMD_EXPORT_ALIAS(ver, VER, read device para);

uint8_t rdp_TXBuffer[50] = {0}; // rdp回复字符串。用于发送

void RdpHandleWBU(uint8_t RdpIndex) {

  uint8_t DecValASCII[8];

  uint16_t regValue = PST_data[RdpIndex];

  memset(rdp_TXBuffer, '\0', sizeof(rdp_TXBuffer));

  // for (uint8_t i = 100; i < 115; i++) {
  //   rt_kprintf("[%d]=%d\r\n", i, PST_data[i]);
  // }

  if (RdpIndex >= 100 &&
      RdpIndex <=
          115) { // 回复[数据ID]=数值，例如数据ID是100的数值为200，返回格式为[100]=200,
    strcpy((char *)rdp_TXBuffer, "[");
    Dec_ASCII_BCD(RdpIndex, 0, DecValASCII);
    strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
    strcat((char *)rdp_TXBuffer, "]=");
    Dec_ASCII_BCD(regValue, 0, DecValASCII);
    strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
  }
}

int rdp(int argc, char **argv) {

  uint8_t index = 1;
  uint16_t RdpIndex = 0;
  uint16_t regValue = 0;

  if (argc == 1) {
    goto __usage;
  }

  if (argc > 3) {
    goto __usage;
  }

  if (strcmp(argv[1], "-d") == 0) {
    index = 2;
  }

  if (strcmp(argv[index], "-h") == 0)
    goto __usage;

  if (index == 1) {
    RdpIndex = atoi(argv[1]);
  } else if (index == 2) {
    RdpIndex = atoi(argv[2]);
    modbus_read_regs(g_ctx, RdpIndex, 1, PST_data + RdpIndex,
                     ARRAY_SIZE(PST_data));
  }

  RdpHandleWBU(RdpIndex);

  rt_kprintf("%s\r\n", rdp_TXBuffer);
  rt_memset(rdp_TXBuffer, '\0', sizeof(rdp_TXBuffer));

  return 0;

__usage:
  rt_kprintf("set reg addr\r\n");
  return 0;
}

// 导出两个命令，共用同一个处理函数
MSH_CMD_EXPORT(rdp, read para);
MSH_CMD_EXPORT_ALIAS(rdp, RDP, read para);

rt_err_t WrpHandleWBU(uint8_t WrpIndex, uint16_t WrpValue) {

  uint8_t DecValASCII[8];

  rt_err_t ret = RT_EOK;
  struct MB_REQ rtu_write;
  rt_memset(rtu_write.data, 0, sizeof(AGILE_MODBUS_MAX_WRITE_REGISTERS));

  rtu_write.start_addr = WrpIndex;
  rtu_write.wrRegQuantity = 1;
  rtu_write.data[0] = WrpValue;
  ret = rt_mq_send(&rtu_req_mq, &rtu_write, sizeof(struct MB_REQ));

  // struct MB_RSP mb_rsp;

  // // 等待回复
  // ret = rt_mq_recv(&rtu_rsp_mq, &mb_rsp, sizeof(struct MB_RSP),
  //                  RT_TICK_PER_SECOND);
  // if (ret != RT_EOK) {
  //   // 没有收到
  //   if (ret == -RT_ETIMEOUT) {
  //     // 超时了
  //     // rt_kprintf("wrp timeout\r\n");
  //   }
  //   return ret;
  // }

  // if (rt_strcmp(mb_rsp.msg, "OK") != 0 || mb_rsp.addr != WrpIndex ||
  //     mb_rsp.value != WrpValue) {
  //   return ret;
  // }

  // rt_kprintf("addr:%d value:%d msg:%s", mb_rsp.addr, mb_rsp.value,
  //            (char *)mb_rsp.msg);

  strcpy((char *)rdp_TXBuffer, "[");
  Dec_ASCII_BCD(WrpIndex, 0, DecValASCII);
  strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
  strcat((char *)rdp_TXBuffer, "]=");
  Dec_ASCII_BCD(WrpValue, 0, DecValASCII);
  strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
  // strcat((char *)rdp_TXBuffer, "\r\n");

  // modbus_write_regs(g_ctx, rtu_write.start_addr, rtu_write.wrRegQuantity,
  //                   rtu_write.data);

  // modbus_read_regs(g_ctx, rtu_write.start_addr, 1,
  //                  PST_data + rtu_write.start_addr, ARRAY_SIZE(PST_data));
  return ret;
}

int wrp(int argc, char **argv) {
  if (argc == 1) {
    goto __usage;
  }

  // rt_kprintf("argc:%d\r\n", argc);

  if (argc > 4 || argc < 2) {
    goto __usage;
  }

  uint16_t WrpIndex = atoi(argv[1]);
  uint16_t WrpValue = atoi(argv[2]);

  rt_err_t ret = WrpHandleWBU(WrpIndex, WrpValue);
  if (ret != RT_EOK) {
  }

  rt_kprintf("%s\r\n", rdp_TXBuffer);
  rt_memset(rdp_TXBuffer, '\0', sizeof(rdp_TXBuffer));

  return ret;

__usage:
  rt_kprintf("set reg address and value\r\n");
  return 0;
}

MSH_CMD_EXPORT(wrp, read para);
MSH_CMD_EXPORT_ALIAS(wrp, WRP, write para);

// sta固定字符串
static uint8_t sta_labelWBU[26][40] = {
    "/",      //[0]--1/2
    "/",      //[1]--2/3
    " ",      //[2]
    ":",      //[3]--4/5
    ":",      //[4]--5/6
    " MODE:", //[5]---7
    " MR:",   //[6]---8
    " MC:",   //[7]---8
    " TS1:",  //[8]---9
    " FS1:",  //[9]---10
    " TS2:",  //[10]---11
    " FS2:",  //[11]---12
    " Bit:",  //[12]---13
    ":",      //[13]---14
    ":",      //[14]---15
    " WBU",   //[15]
};

char pStaChr[512] = {"02/01/00 20:25:48 MODE:1 MR:1 TS1:200 FS1:70 TS2:200 "
                     "FS2:70 Bit:8000:0000:0000 WBU"};

void HEX_ASCII_BCD(uint16_t Dec, uint8_t HexValASCII[8]) {
  uint16_t temp2;
  int16_t BitVal0, BitVal1, BitVal2, BitVal3;
  static uint8_t temp4[5] = {"0000"};
  temp2 = Dec;
  BitVal0 = temp2 / 0x1000;
  if (BitVal0 >= 0 && BitVal0 <= 9)
    temp4[0] = BitVal0 + '0';
  else if (BitVal0 >= 10 && BitVal0 <= 15)
    temp4[0] = BitVal0 - 10 + 'A';

  BitVal1 = temp2 % 0x1000 / 0x100;
  if (BitVal1 >= 0 && BitVal1 <= 9)
    temp4[1] = BitVal1 + '0';
  else if (BitVal1 >= 10 && BitVal1 <= 15)
    temp4[1] = BitVal1 - 10 + 'A';

  BitVal2 = temp2 % 0x100 / 0x10;
  if (BitVal2 >= 0 && BitVal2 <= 9)
    temp4[2] = BitVal2 + '0';
  else if (BitVal2 >= 10 && BitVal2 <= 15)
    temp4[2] = BitVal2 - 10 + 'A';
  BitVal3 = temp2 % 0x10;
  if (BitVal3 >= 0 && BitVal3 <= 9)
    temp4[3] = BitVal3 + '0';
  else if (BitVal3 >= 10 && BitVal3 <= 15)
    temp4[3] = BitVal3 - 10 + 'A';
  temp4[4] = '\0';
  strcpy((char *)HexValASCII, (const char *)temp4);
}

void StaHandleWBU() /// WBU机组sta字符串处理函数(状态查询回复字符串处理函数)
{
  uint8_t HexValASCII[8];
  uint8_t DecValASCII[8];
  memset(pStaChr, '\0', sizeof(pStaChr));
  Dec_ASCII_BCD(PST_data[61], 2, DecValASCII);

  strcpy((char *)pStaChr, (char const *)DecValASCII); // 日

  strcat((char *)pStaChr, (char const *)sta_labelWBU[0]); //"/"

  Dec_ASCII_BCD(PST_data[62], 2, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII); // 月

  strcat((char *)pStaChr, (char const *)sta_labelWBU[1]); //"/"

  if (PST_data[63] > 2000)
    PST_data[63] = PST_data[63] - 2000;

  if (PST_data[63] > 99)
    PST_data[63] = 99;

  Dec_ASCII_BCD(PST_data[63], 2, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII); // 年

  strcat((char *)pStaChr, (char const *)sta_labelWBU[2]); //" "

  Dec_ASCII_BCD(PST_data[64], 2, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII); // 时

  strcat((char *)pStaChr, (char const *)sta_labelWBU[3]); //":"
  Dec_ASCII_BCD(PST_data[65], 2, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII); // 分

  strcat((char *)pStaChr, (char const *)sta_labelWBU[4]); //":"

  Dec_ASCII_BCD(PST_data[66], 2, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII); // 秒

  strcat((char *)pStaChr, (char const *)sta_labelWBU[5]); //" MODE:"
  Dec_ASCII_BCD(PST_data[7], 0, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII); //

  strcat((char *)pStaChr, (char const *)sta_labelWBU[6]); //" MR:",
  Dec_ASCII_BCD(PST_data[8], 0, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII); //

  // strcat((char *)pStaChr,
  //        (char const *)sta_labelWBU[7]); //"
  //        MC:",2026-04-16协议，2026-04-20增加
  // Dec_ASCII_BCD(PST_data[6], 0, DecValASCII);
  // strcat((char *)pStaChr, (char const *)DecValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[8]); //"  TS1:",
  Dec_ASCII_BCD(PST_data[9], 0, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[9]); //"  FS1:,
  Dec_ASCII_BCD(PST_data[10], 0, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[10]); //" TS2:",
  Dec_ASCII_BCD(PST_data[11], 0, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[11]); //" FS2:",
  Dec_ASCII_BCD(PST_data[12], 0, DecValASCII);
  strcat((char *)pStaChr, (char const *)DecValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[12]); //" Bit:",
  HEX_ASCII_BCD(PST_data[15], HexValASCII);
  strcat((char *)pStaChr, (char const *)HexValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[13]); //":",
  HEX_ASCII_BCD(PST_data[14], HexValASCII);
  strcat((char *)pStaChr, (char const *)HexValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[14]); //":",
  HEX_ASCII_BCD(PST_data[13], HexValASCII);
  strcat((char *)pStaChr, (char const *)HexValASCII);

  strcat((char *)pStaChr, (char const *)sta_labelWBU[15]); //"WBU",
}

int sta(int argc, char **argv) {

  StaHandleWBU();
  rt_kprintf("%s\r\n", pStaChr);
  return 0;
}
MSH_CMD_EXPORT(sta, get status);
MSH_CMD_EXPORT_ALIAS(sta, STA, get status);

static bool ALARM_OK = false;
static bool ALARM_GetOffOK = false;
static bool ALARM_GetOnOK = false;

static char buffer_SendCommandErr[4352] = {0};
static char buffer_SendCommandGetOffErr[3840] = {
    0}; // 报警触发恢复内容字符串，主动上传
static uint8_t NoalarmStr[20] = "NO ALARM";

static uint8_t AlarmFlag[120] = {0}; // 当前64个报警状态
static uint8_t AlarmFlagPre[120] = {
    0}; // 之前64个报警状态，如果之前报警状态为0，当前报警状态为1，则意味着产生新的报警，将发送Error报警。

char AlarmDescriptionWBU[64][60] = // 64为报警个数，60为描述长度
    {
        "Warning - REF Chiller temperature too low",
        "Warning - REF Chiller temperature too high",
        "Warning - Main Chiller temperature too low",
        "Warning - Main Chiller temperature too high",
        "Error - REF Chiller temperature too low",
        "Error - REF Chiller temperature too high",
        "Error - Main Chiller temperature too low",
        "Error - Main Chiller temperature too high",
        "Warning - REF Chiller flow rate too low",
        "Warning - REF Chiller flow rate too high",
        "Warning - Main Chiller flow rate too low",
        "Warning - Main Chiller flow rate too high",
        "Error - REF Chiller flow rate too low",
        "Error - REF Chiller flow rate too high",
        "Error - Main Chiller flow rate too low",
        "Error - Main Chiller flow rate too high",
        "Error - REF Chiller temperature sensor failure",
        "Error - REF Chiller flow sensor failure",
        "Error - Main Chiller temperature sensor failure",
        "Error - Main Chiller flow sensor failure",
        "Error - REF Chiller Input 2-way actuator failure",
        "Error - REF Chiller Output 2-way actuator failure",
        "Error - REF Chiller Bypass 2-way actuator failure",
        "Error - Main Chiller Input 2-way actuator failure",
        "Error - Main Chiller Output 2-way actuator failure",
        "Error - Main Chiller Bypass 2-way actuator failure",
        "Warning - back up mode",
        "Warning - MR system off",
        "Warning - Main Chiller off",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Initialisation - please wait", // INI  -------------------------16
};

char AlarmGetOffDescriptionWBU[64][60] = // 64为报警个数，60为描述长度
    {
        "Get off - REF Chiller temperature too low",
        "Get off - REF Chiller temperature too high",
        "Get off - Main Chiller temperature too low",
        "Get off - Main Chiller temperature too high",
        "Get off - REF Chiller temperature too low",
        "Get off - REF Chiller temperature too high",
        "Get off - Main Chiller temperature too low",
        "Get off - Main Chiller temperature too high",
        "Get off - REF Chiller flow rate too low",
        "Get off - REF Chiller flow rate too high",
        "Get off - Main Chiller flow rate too low",
        "Get off - Main Chiller flow rate too high",
        "Get off - REF Chiller flow rate too low",
        "Get off - REF Chiller flow rate too high",
        "Get off - Main Chiller flow rate too low",
        "Get off - Main Chiller flow rate too high",
        "Get off - REF Chiller temperature sensor failure",
        "Get off - REF Chiller flow sensor failure",
        "Get off - Main Chiller temperature sensor failure",
        "Get off - Main Chiller flow sensor failure",
        "Get off - REF Chiller Input 2-way actuator failure",
        "Get off - REF Chiller Output 2-way actuator failure",
        "Get off - REF Chiller Bypass 2-way actuator failure",
        "Get off - Main Chiller Input 2-way actuator failure",
        "Get off - Main Chiller Output 2-way actuator failure",
        "Get off - Main Chiller Bypass 2-way actuator failure",
        "Get off - back up mode",
        "Get off - MR system off",
        "Get off - Main Chiller off",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Unit On",
};

void SendErrorGetOffStr() {
  uint8_t ss, k, nn;
  uint16_t err_TXBuffer[4];
  memset(err_TXBuffer, 0, sizeof(err_TXBuffer));
  err_TXBuffer[0] = PST_data[13];
  err_TXBuffer[1] = PST_data[14];
  err_TXBuffer[2] = PST_data[15];
  memset(buffer_SendCommandGetOffErr, '\0',
         sizeof(buffer_SendCommandGetOffErr));
  strcpy(buffer_SendCommandGetOffErr, "");
  nn = 0;
  ALARM_GetOffOK = false;
  ALARM_GetOnOK = false;
  /// 先整理更新所有新报警发送信息（error code 信息字符串）
  for (ss = 0; ss < 3; ss++) {
    for (k = 0; k < 16; k++) {
      nn++;
      if (((err_TXBuffer[ss] >> k) & 0x01) == 0x01 && nn < 48) {
        AlarmFlag[nn] = 1; // 保存当前的报警状态；
        if (UnitType == 2 && AlarmFlag[nn] == 1 && AlarmFlagPre[nn] == 0) {
          strcat(buffer_SendCommandGetOffErr,
                 AlarmDescriptionWBU
                     [(ss * 16) +
                      k]); // 有新报警发生，在AlarmGetoff字符串中添加新报警内容
          ALARM_GetOnOK = true;
          strcat(buffer_SendCommandGetOffErr, "\r\n");
        }
      } else {
        AlarmFlag[nn] = 0;
      }
    }
  }
  /// 后整理更新所有复位报警发送信息（get off error code信息字符串）
  for (nn = 1; nn < 48; nn++) {
    if (UnitType == 2 && AlarmFlagPre[nn] == 1 && AlarmFlag[nn] == 0) {
      strcat(buffer_SendCommandGetOffErr,
             AlarmGetOffDescriptionWBU
                 [nn - 1]); // 有报警复位,在AlarmGetoff字符串中添加报警复位内容
      strcat(buffer_SendCommandGetOffErr, "\r\n");
      ALARM_GetOffOK = true;
    }
    AlarmFlagPre[nn] = AlarmFlag[nn]; // 保存之前的报警状态；
  }

  if (ALARM_GetOffOK == true ||
      ALARM_GetOnOK == true) /// 有报警状态变化更新sta信息字符串
  {
    if (UnitType == 2) {
      StaHandleWBU();
    }
    strcat((char *)buffer_SendCommandGetOffErr, (const char *)pStaChr);
    if (strlen(buffer_SendCommandGetOffErr) >
        0) // 如果右报警触发或报警复位信息则通过console口发送报警信息
    {
      rt_kprintf("%s", buffer_SendCommandGetOffErr);
      // uart1_puts(buffer_SendCommandGetOffErr);
      uart6_puts(buffer_SendCommandGetOffErr);
    }
  }
  memset(buffer_SendCommandGetOffErr, '\0',
         sizeof(buffer_SendCommandGetOffErr)); // 发送AlamrGetoff内容
}

void SendErrorStr() {
  uint8_t ss, k, nn;
  uint16_t err_TXBuffer[4];
  memset(err_TXBuffer, 0, sizeof(err_TXBuffer));
  err_TXBuffer[0] = PST_data[13];
  err_TXBuffer[1] = PST_data[14];
  err_TXBuffer[2] = PST_data[15];
  memset(buffer_SendCommandErr, '\0', sizeof(buffer_SendCommandErr));
  strcpy(buffer_SendCommandErr, "");
  nn = 0;
  ALARM_OK = false;
  for (ss = 0; ss < 3; ss++) // 3个整型数
  {
    for (k = 0; k < 16; k++) // 每个整型数16位，
    {
      nn++;
      if (((err_TXBuffer[ss] >> k) & 0x01) == 0x01 &&
          nn <
              48) // 总共47个报警，最后一个（第48位）不是报警，每一位等于1对应一个AlarmDescriptionWBU字符串数组
      {
        AlarmFlag[nn] = 1;
        AlarmFlagPre[nn] = 1;
        ALARM_OK = true;
        if (UnitType == 2) {
          strcat(buffer_SendCommandErr, AlarmDescriptionWBU[(ss * 16) + k]);
        }
        // strcat(buffer_SendCommandErr, "\r\n");
      } else {
        AlarmFlag[nn] = 0;
        AlarmFlagPre[nn] = 0;
      }
    }
  }
}

int err(int argc, char **argv) {
  SendErrorStr();
  if (ALARM_OK == false) {
    rt_kprintf("%s\r\n", NoalarmStr);
  } else {
    rt_kprintf("%s\r\n", buffer_SendCommandErr);
  }
}

MSH_CMD_EXPORT(err, get errors);
MSH_CMD_EXPORT_ALIAS(err, ERR, get errors);

int msh_split(char *cmd, rt_size_t length, char *argv[FINSH_ARG_MAX]) {
  char *ptr;
  rt_size_t position;
  rt_size_t argc;
  rt_size_t i;

  ptr = cmd;
  position = 0;
  argc = 0;

  while (position < length) {
    /* strip bank and tab */
    while ((*ptr == ' ' || *ptr == '\t') && position < length) {
      *ptr = '\0';
      ptr++;
      position++;
    }

    if (argc >= FINSH_ARG_MAX) {
      rt_kprintf("Too many args ! We only Use:\n");
      for (i = 0; i < argc; i++) {
        rt_kprintf("%s ", argv[i]);
      }
      rt_kprintf("\n");
      break;
    }

    if (position >= length)
      break;

    /* handle string */
    if (*ptr == '"') {
      ptr++;
      position++;
      argv[argc] = ptr;
      argc++;

      /* skip this string */
      while (*ptr != '"' && position < length) {
        if (*ptr == '\\') {
          if (*(ptr + 1) == '"') {
            ptr++;
            position++;
          }
        }
        ptr++;
        position++;
      }
      if (position >= length)
        break;

      /* skip '"' */
      *ptr = '\0';
      ptr++;
      position++;
    } else {
      argv[argc] = ptr;
      argc++;
      while ((*ptr != ' ' && *ptr != '\t') && position < length) {
        ptr++;
        position++;
      }
      if (position >= length)
        break;
    }
  }

  return argc;
}

void cmd_handler(rt_device_t dev, char *buf, uint8_t len) {

  char *argv[FINSH_ARG_MAX];
  int argc;

  /* 1. 预处理：去掉结尾的换行符 (非常重要！) */
  char *pos;
  if ((pos = strchr(buf, '\r')) != RT_NULL)
    *pos = '\0';
  if ((pos = strchr(buf, '\n')) != RT_NULL)
    *pos = '\0';

  if (argc = msh_split(buf, len, argv)) {
    /* 3. 解析成功，开始处理 argc/argv */
    // log_d("Total args: %d\n", argc);

    // 打印所有解析出来的参数
    // for (int i = 0; i < argc; i++) {
    //   rt_kprintf("argv[%d]: %s\n", i, argv[i]);
    // }

    /* 4. 业务逻辑处理 */
    if (argc == 0)
      return;

    if (rt_strlen(argv[0]) != 3) {
      return;
    }

    // char temp[128];
    // sprintf(temp, "argc:%d\r\n", argc);
    // rt_device_write(dev, 0, temp, rt_strlen(temp));

    // 比较第一个参数 (命令名)
    if (rt_strcmp(argv[0], "ver") == 0 || rt_strcmp(argv[0], "VER") == 0) {
      if (argc == 1) {

        VerHandleWBU();
        rt_device_write(dev, 0, ver_TXBuffer, rt_strlen(ver_TXBuffer));
      }
      if (argc == 2) {
        uint8_t input_lrc = ascii_to_uint8((const uint8_t *)argv[1]);
        if (!check_lrc(argv[0], input_lrc))
          return;

        VerHandleWBU();
        char output[512];
        uint8_t out_lrc =
            calculate_lrc((uint8_t *)ver_TXBuffer, strlen(ver_TXBuffer));

        char lrc_ascii[3] = {0};
        uint8_to_ascii(out_lrc, lrc_ascii);

        sprintf(output, "%s %s\r\n", ver_TXBuffer, lrc_ascii);
        rt_device_write(dev, 0, output, rt_strlen(output));
      }

    } else if (rt_strcmp(argv[0], "sta") == 0 ||
               rt_strcmp(argv[0], "STA") == 0) {

      if (argc == 1) {
        StaHandleWBU();
        rt_device_write(dev, 0, pStaChr, rt_strlen(pStaChr));
        return;
      }
      if (argc == 2) {
        uint8_t input_lrc = ascii_to_uint8((const uint8_t *)argv[1]);
        if (!check_lrc(argv[0], input_lrc))
          return;

        StaHandleWBU();
        char output[512];
        uint8_t out_lrc = calculate_lrc((uint8_t *)pStaChr, strlen(pStaChr));

        char lrc_ascii[3] = {0};
        uint8_to_ascii(out_lrc, lrc_ascii);

        sprintf(output, "%s %s\r\n", pStaChr, lrc_ascii);

        rt_device_write(dev, 0, output, rt_strlen(output));
      }

    } else if (rt_strcmp(argv[0], "err") == 0 ||
               rt_strcmp(argv[0], "ERR") == 0) {

      if (argc == 1) {
        SendErrorStr();
        if (ALARM_OK == false) {

          rt_device_write(dev, 0, NoalarmStr, rt_strlen(NoalarmStr));

        } else {
          rt_device_write(dev, 0, buffer_SendCommandErr,
                          rt_strlen(buffer_SendCommandErr));
        }
      } else if (argc == 2) {
        uint8_t input_lrc = ascii_to_uint8((const uint8_t *)argv[1]);
        if (!check_lrc(argv[0], input_lrc))
          return;

        SendErrorStr();

        char output[512];

        if (ALARM_OK == false) {

          uint8_t out_lrc =
              calculate_lrc((uint8_t *)NoalarmStr, strlen(NoalarmStr));

          char lrc_ascii[3] = {0};
          uint8_to_ascii(out_lrc, lrc_ascii);

          sprintf(output, "%s %s\r\n", NoalarmStr, lrc_ascii);
          rt_device_write(dev, 0, output, rt_strlen(output));

          // rt_device_write rt_device_write(dev, 0, NoalarmStr,
          //                                 rt_strlen(NoalarmStr));

        } else {
          uint8_t out_lrc = calculate_lrc((uint8_t *)buffer_SendCommandErr,
                                          strlen(buffer_SendCommandErr));

          char lrc_ascii[3] = {0};
          uint8_to_ascii(out_lrc, lrc_ascii);

          sprintf(output, "%s %s\r\n", buffer_SendCommandErr, lrc_ascii);
          rt_device_write(dev, 0, output, rt_strlen(output));

          // rt_device_write(dev, 0, buffer_SendCommandErr,
          //                 rt_strlen(buffer_SendCommandErr));
        }
      }

    } else if (rt_strcmp(argv[0], "rdp") == 0 ||
               rt_strcmp(argv[0], "RDP") == 0) {

      uint16_t RdpIndex = atoi(argv[1]);

      char output[1024] = {0};
      if (argc == 2) {
        RdpHandleWBU(RdpIndex);
        sprintf(output, "%s\r\n", rdp_TXBuffer);
        rt_device_write(dev, 0, output, rt_strlen(output));
      }

      if (argc == 3) {
        uint8_t input_lrc = ascii_to_uint8((const uint8_t *)argv[2]);

        char temp[100] = {0};
        sprintf(temp, "%s %s", argv[0], argv[1]);
        if (!check_lrc(temp, input_lrc))
          return;

        RdpHandleWBU(RdpIndex);
        uint8_t out_lrc =
            calculate_lrc((uint8_t *)rdp_TXBuffer, strlen(rdp_TXBuffer));

        char lrc_ascii[3] = {0};
        uint8_to_ascii(out_lrc, lrc_ascii);
        sprintf(output, "%s %s\r\n", rdp_TXBuffer, lrc_ascii);
        rt_device_write(dev, 0, output, rt_strlen(output));
      }

      rt_memset(rdp_TXBuffer, '\0', sizeof(rdp_TXBuffer));
    } else if (rt_strcmp(argv[0], "wrp") == 0 ||
               rt_strcmp(argv[0], "WRP") == 0) {

      uint16_t WrpIndex = 0;
      uint16_t WrpValue = 0;

      WrpIndex = atoi(argv[1]);
      WrpValue = atoi(argv[2]);
      char temp[100] = {0};
      char output[1024] = {0};

      if (argc == 3) {
        rt_err_t ret = WrpHandleWBU(WrpIndex, WrpValue);
        if (ret != RT_EOK) {
          // sprintf(temp, "%s", "WRP ERR\r\n");
          // rt_device_write(dev, 0, temp, rt_strlen(temp));
          return;
        } else {
          rt_device_write(dev, 0, rdp_TXBuffer, rt_strlen(rdp_TXBuffer));
        }
      }

      if (argc == 4) {
        uint8_t input_lrc = ascii_to_uint8((const uint8_t *)argv[3]);

        char temp[100] = {0};
        sprintf(temp, "%s %s %s", argv[0], argv[1], argv[2]);
        if (!check_lrc(temp, input_lrc))
          return;

        rt_err_t ret = WrpHandleWBU(WrpIndex, WrpValue);

        if (ret != RT_EOK) {
          // sprintf(temp, "%s", "WRP ERR\r\n");
          // rt_device_write(dev, 0, temp, rt_strlen(temp));
          return;
        } else {
          uint8_t out_lrc =
              calculate_lrc((uint8_t *)rdp_TXBuffer, strlen(rdp_TXBuffer));

          char lrc_ascii[3] = {0};
          uint8_to_ascii(out_lrc, lrc_ascii);
          sprintf(output, "%s %s\r\n", rdp_TXBuffer, lrc_ascii);

          rt_device_write(dev, 0, output, rt_strlen(output));
        }
      }

      rt_memset(rdp_TXBuffer, '\0', sizeof(rdp_TXBuffer));
    }
  }
}

int reg(int argc, char **argv) {
  LOG_HEX("PST_data", 20, (unsigned char *)PST_data, READ_HOLDING_CNT * 2);
}

MSH_CMD_EXPORT(reg, get holdings);
MSH_CMD_EXPORT_ALIAS(reg, REG, get holdings);

int cfg(int argc, char **argv) {
  LOG_HEX("PST_data", 20, (unsigned char *)PST_data, READ_HOLDING_CNT * 2);
  if (argc == 0) {

    return 0;
  }
}

MSH_CMD_EXPORT(cfg, get holdings);
MSH_CMD_EXPORT_ALIAS(cfg, CFG, get holdings);
