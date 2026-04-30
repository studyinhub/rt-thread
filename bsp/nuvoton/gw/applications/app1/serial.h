
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "inc/hw_ints.h"
#include <math.h>
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/timer.h"
#include "config.h"
#include "serial.h"

extern uint32_t ui32SysClock;
//*****************************************************************************
//
//! \addtogroup serial_api
//! @{
//
//*****************************************************************************
static uint8_t g_pucRX0Buffer[512]; // RS232接收缓存
static uint8_t g_pucRX1Buffer[512]; // RS485接收缓存
static uint8_t g_pucRX2Buffer[512]; // Console接收缓存
static uint32_t ModbusCmd = 0;
static uint32_t ModbusCmdFlag = 0;
static uint8_t UnitType = 5;

static uint16_t ModbusStartAdd[2] = {0x01,
                                     0x3D}; // Modbus定时查询起始地址--1，61
static uint16_t ModbusSDataLeth[2] = {0x46,
                                      0x46}; // Modbus定时查询数据长度--70，70
uint8_t Temp_Command1[10] = {0x01, 0x03, 0x00,
                             0x01, 0x00, 0x46}; // 第1条modbus读取控制器数据指令
uint8_t Temp_Command2[10] = {0x01, 0x03, 0x00,
                             0x3D, 0x00, 0x46}; // 第2条modbus读取控制器数据指令
static uint16_t SWCommdValue = 0;
static uint16_t SWCommdIDWRP = 0;
static uint8_t NoalarmStr[20] = "NO ALARM\r\n";
static uint16_t SWCommdTime2 = 0;
static uint16_t SWCommdTime2Times = 0;
static uint8_t Mbustemp[10];
// static uint32_t STA_Flag=0;
static uint32_t SysSTA = 0;
static uint32_t SysSTAPre = 0;
static uint32_t SysSTAYear = 0;
static uint32_t SysSTAMonth = 0;
static uint32_t SysSTADay = 0;
static uint32_t SysSTAHour = 0;
static uint32_t SysSTAMinute = 0;
static uint32_t SysSTASecond = 0;
static uint8_t SysStart = 0;

static bool ALARM_OK = false;
static bool ALARM_GetOffOK = false;
static bool ALARM_GetOnOK = false;
static char buffer_SendCommandGetOffErr[3840] = {
    0}; // 报警触发恢复内容字符串，主动上传
static char buffer_SendCommandErr[4352] = {
    0}; // 报警触发内容字符串,被动回复EER指令

static const uint32_t g_ulUARTBase[MAX_S2E_PORTS] = {UART0_BASE, UART1_BASE,
                                                     UART2_BASE};
/*
static const uint32_t g_ulUART485Base =
{
    PIN_U485_PORT
};
static const uint32_t g_ul485OutPin =
{
    PIN_U485_PIN
};
*/

static uint32_t i = 0, j = 0, l = 0;

//****************************************************************************
// void此定时函数取消
// ConsoleTimerHandler(void)//Console定时器调用函数定时器为500ms，每500ms执行一次此函数
//{
//    uint32_t ulStatus;
//    uint16_t Send_len;
//    if(SysSTA-SysSTAPre>=g_sParameters.s485Port.staTimer)//
//    控制器实时时间间隔达到STA主动上传间隔（1~3600秒），则发送STA响应
//    {
//        SysSTAPre=SysSTA;
//        STA_Flag=0;//定制发送标记=0正在发送定时STA字符串，不允许RS232端口响应别的指令
//        if(UnitType==2)
//        {
//            StaHandleWBU();
//        }
//        memset(sta_TXBuffer,'\0',sizeof(sta_TXBuffer)); //清除发送STA字符串
//        strcpy((char *)sta_TXBuffer,(char *)pStaChr);
//        //将sta缓存拷贝到STA发送字符串 Send_len=strlen((char *)sta_TXBuffer);
//        //计算STA发送字符串长度 if (Send_len>512)
//            Send_len=512;
//        if (Send_len>0)
//            UARTSend(2, sta_TXBuffer, Send_len);//RS232端口发送STA字符串，
//        STA_Flag=1;//定制发送标记=1定时发送STA字符串结束，允许RS232端口响应别的指令
//    }
//    else
//        STA_Flag=1;//定制发送标记=1RS3232定时发送STA字符串空闲，允许RS232端口响应别的指令
//}

//-------Get On Alarm Information-----/////////////
///////////////////////////////////////////////////发送报警内容函数--触发报警内容字符串（响应SVU从RS232或PC从Consoled端口用EER指令查询报警的回复字符串处理函数）
// RS232接收中断处理函数RS232ManageHandlerWBU中调用,Console接收中断处理函数ConsoleManageHandlerWBU中调用

//-------Get OnOff Alarm Information-----/////////////
///////////////////////////////////////////////////发送报警触发报警恢复内容内容字符串函数--（Console口主动上传故障信息字符串处理函数，放在RS485定时器函数RS485IntHandler中）

//************************************************************************** */
void RS232ManageHandlerWBU(
    void) // RS232中断调用， RS232为从站，响应SUV上位机指令处理函数
{
  char *p;
  char *pRX0Buffer;                // 接收字符串中符合协议指令的字符串段
  uint8_t rdp_TXBuffer[50] = {0};  // rdp回复字符串。用于发送
  uint8_t sta_TXBuffer[512] = {0}; // sta回复字符串。用于发送
  uint8_t ver_TXBuffer[512] = {0}; // ver回复字符串。用于发送
  uint8_t DecValASCII[8];
  uint16_t RdpIndex = 0;
  uint16_t statime;
  uint16_t Send_len;

  // 一共五条命令 sta ver err rdp wrp
  if (strstr((const char *)g_pucRX0Buffer, "STA") ||
      strstr((const char *)g_pucRX0Buffer, "sta")) // STA指令，机组状态查询指令
  {
    if (strstr((const char *)g_pucRX0Buffer, "STA"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX0Buffer, "STA");
    else if (strstr((const char *)g_pucRX0Buffer, "sta"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX0Buffer, "sta");
    if (*(pRX0Buffer + 3) == 0x0D ||
        *(pRX0Buffer + 3) ==
            0x0A) // 如果接收指令只有sta\r\n,则直接返回sta字符串
    {
      StaHandleWBU();                                   // 处理sta字符串
      memset(sta_TXBuffer, '\0', sizeof(sta_TXBuffer)); // 清除sta发送字符串
      strcpy((char *)sta_TXBuffer,
             (char *)pStaChr); // 拷贝sta字符串到sta发送字符串
      Send_len = strlen((char *)sta_TXBuffer);
      if (Send_len > 512)
        Send_len = 512;
      if (Send_len > 0)
        UARTSend(0, sta_TXBuffer, Send_len); // RS232端口发送sta发送字符串
    } else if (*(pRX0Buffer + 3) > '0' &&
               *(pRX0Buffer + 3) <=
                   '9') // 如果接收指令为Sta+数值则用数值更新RS定时间隔系统变量
    {
      p = (pRX0Buffer + 3);
      statime = (uint16_t)atoi((char const *)p);
      g_sParameters.s485Port.staTimer = statime; // 更新RS定时间隔
      Configwrite();                             // 保存RS232定时间隔变量
      ConfigSave();                              // 保存RS232定时间隔变量
    } else if (
        *(pRX0Buffer + 3) == ' ' && *(pRX0Buffer + 4) > '0' &&
        *(pRX0Buffer + 4) <=
            '9') // 如果接收指令为Sta+空格+数值则用数值更新RS定时间隔系统变量
    {
      p = (pRX0Buffer + 4);
      statime = (uint16_t)atoi((char const *)p);
      g_sParameters.s485Port.staTimer = statime; // 更新RS485定时间隔
      Configwrite();                             // 保存RS232定时间隔变量
      ConfigSave();                              // 保存RS232定时间隔变量
    }
  } else if (strstr((const char *)g_pucRX0Buffer, "VER") ||
             strstr((const char *)g_pucRX0Buffer,
                    "ver")) // VER指令，版本查询指令
  {

    VerHandleWBU();
    memset(ver_TXBuffer, '\0', sizeof(ver_TXBuffer));
    strcpy((char *)ver_TXBuffer, (char const *)pVerChr);
    Send_len = strlen((char *)ver_TXBuffer);
    if (Send_len > 512)
      Send_len = 512;
    if (Send_len > 0)
      UARTSend(0, ver_TXBuffer, Send_len);
  }

  else if (strstr((const char *)g_pucRX0Buffer, "ERR") ||
           strstr((const char *)g_pucRX0Buffer, "err")) // ERR指令，报警查询指令
  {
    SendErrorStr();        // 处理报警信息
    if (ALARM_OK == false) // 如果没有报警发生，则返回NO ALARM
    {
      UARTSend(2, NoalarmStr, 10);
    } else if (strlen(buffer_SendCommandErr) >
               2) // 否则返回报警信息内容buffer_SendCommandErr
      UARTSend(0, (uint8_t const *)buffer_SendCommandErr,
               strlen(buffer_SendCommandErr));
    ALARM_OK = false;
  } else if (strstr((const char *)g_pucRX0Buffer, "RDP") ||
             strstr((const char *)g_pucRX0Buffer,
                    "rdp")) // RDP指令，参数读取指令
  { // 接收rpd+空格+数据ID，例如rdp 100,读取数据ID是100的数值
    if (strstr((const char *)g_pucRX0Buffer, "RDP"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX0Buffer, "RDP");
    else if (strstr((const char *)g_pucRX0Buffer, "rdp"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX0Buffer, "rdp");
    memset(rdp_TXBuffer, '\0', sizeof(rdp_TXBuffer));
    p = (pRX0Buffer + 4);                       // 接收数组第5个缓存起是数据ID
    RdpIndex = (uint16_t)atoi((char const *)p); // 获取数据ID
    if (RdpIndex >= 100 &&
        RdpIndex <=
            115) { // 回复[数据ID]=数值，例如数据ID是100的数值为200，返回格式为[100]=200,
      strcpy((char *)rdp_TXBuffer, "[");
      Dec_ASCII_BCD(RdpIndex, 0, DecValASCII);
      strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
      strcat((char *)rdp_TXBuffer, "]=");
      Dec_ASCII_BCD(PST_data[RdpIndex], 0, DecValASCII);
      strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
    }
    strcat((char *)rdp_TXBuffer, "\r\n");
    Send_len = strlen((char *)rdp_TXBuffer);
    if (Send_len > 50)
      Send_len = 50;
    if (Send_len > 0)
      UARTSend(0, rdp_TXBuffer, Send_len);
  } else if (strstr((const char *)g_pucRX0Buffer, "WRP") ||
             strstr((const char *)g_pucRX0Buffer,
                    "wrp")) // WRP指令，参数写入指令，上位机写到控制器
  { // 接收到的指令格式为wrp+空格+数据ID+空格+数值，例如WRP 100
    // 120,修改数据ID是100的数值为120
    SWCommdIDWRP = 0; // 参数地址（modbus RTU 数据寄存器地址)
    SWCommdValue = 0; // 参数值（modbus RTU 数据寄存器数值)
    if (strstr((const char *)g_pucRX0Buffer, "WRP"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX0Buffer, "WRP");
    else if (strstr((const char *)g_pucRX0Buffer, "wrp"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX0Buffer, "wrp");
    p = (pRX0Buffer + 4);                           // 第5个字节起为数据ID
    SWCommdIDWRP = (uint16_t)atoi((char const *)p); // 获取数据ID
    Mbustemp[0] = 0x01;                             // Devie ID=1
    Mbustemp[1] = 0x06;                             // funtion	=6
    if (SWCommdIDWRP >= 100 && SWCommdIDWRP <= 115) // 数据ID必须在100~115范围内
    {
      if (SWCommdTime2 == 0) // 如果RS485端口是写入状态则可以写入
      {
        SWCommdValue = (uint16_t)atoi(
            (char const *)(pRX0Buffer +
                           8)); // 数据值为接收到的缓存的第9个字节起，获取数据值
        Mbustemp[2] = SWCommdIDWRP / 0x100; // 数据ID高16位
        Mbustemp[3] = SWCommdIDWRP % 0x100; // 数据ID低16位
        Mbustemp[4] = SWCommdValue / 0x100; // 数据值高16位
        Mbustemp[5] = SWCommdValue % 0x100; // 数据值低16位
        ModbusCRC(Mbustemp, 6);             // modbus crc校验
        Mbustemp[6] = g_lowCrcbyte;         // crc
        Mbustemp[7] = g_highCrcbyte;        // crc

        SWCommdTime2 = 1;      // RS485写参数标记置1
        SWCommdTime2Times = 0; // RS485写参数次数复位
        // timedelay(2000000);//RS485延时2秒
        UARTSend(1, Mbustemp, 8); // RS485发送Modbus指令给控制器
        j = 0;                    // 复位RS485接收数据长度
        // ROM_TimerEnable(TIMER2_BASE, TIMER_A);//启动RS485定时器
      }
    }
  }
  memset(g_pucRX0Buffer, 0, sizeof(g_pucRX0Buffer));
  i = 0; // 复位RS232接收数据
}

//************************************************************************** */
void ConsoleManageHandlerWBU(
    void) // Console中断调用， Console为从站,响应PC指令处理函数
{
  char *p;
  char *pRX0Buffer;
  uint8_t rdp_TXBuffer[50] = {0};
  uint8_t sta_TXBuffer[512] = {0};
  uint8_t ver_TXBuffer[512] = {0};
  uint8_t DecValASCII[8];
  uint16_t RdpIndex = 0;
  uint16_t statime;
  uint16_t Send_len;
  // 一共五条命令 sta ver err rdp wrp
  if (strstr((const char *)g_pucRX2Buffer, "STA") ||
      strstr((const char *)g_pucRX2Buffer, "sta")) // STA指令，机组状态查询指令
  {
    if (strstr((const char *)g_pucRX2Buffer, "STA"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX2Buffer, "STA");
    else if (strstr((const char *)g_pucRX2Buffer, "sta"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX2Buffer, "sta");
    if (*(pRX0Buffer + 3) == 0x0D ||
        *(pRX0Buffer + 3) ==
            0x0A) // 如果接收指令只有sta\r\n,则直接返回sta字符串
    {
      StaHandleWBU();                                   // 处理sta字符串
      memset(sta_TXBuffer, '\0', sizeof(sta_TXBuffer)); // 清除sta发送字符串
      strcpy((char *)sta_TXBuffer,
             (char *)pStaChr); // 拷贝sta字符串到sta发送字符串
      Send_len = strlen((char *)sta_TXBuffer);
      if (Send_len > 512)
        Send_len = 512;
      if (Send_len > 0)
        UARTSend(2, sta_TXBuffer, Send_len); // RS232端口发送sta发送字符串
    } else if (*(pRX0Buffer + 3) > '0' &&
               *(pRX0Buffer + 3) <=
                   '9') // 如果接收指令为Sta+数值则用数值更新RS定时间隔系统变量
    {
      p = (pRX0Buffer + 3);
      statime = (uint16_t)atoi((char const *)p);
      g_sParameters.s485Port.staTimer = statime; // 更新RS定时间隔
      Configwrite();                             // 保存RS232定时间隔变量
      ConfigSave();                              // 保存RS232定时间隔变量
    } else if (
        *(pRX0Buffer + 3) == ' ' && *(pRX0Buffer + 4) > '0' &&
        *(pRX0Buffer + 4) <=
            '9') // 如果接收指令为Sta+空格+数值则用数值更新RS定时间隔系统变量
    {
      p = (pRX0Buffer + 4);
      statime = (uint16_t)atoi((char const *)p);
      g_sParameters.s485Port.staTimer = statime; // 更新RS485定时间隔
      Configwrite();                             // 保存RS232定时间隔变量
      ConfigSave();                              // 保存RS232定时间隔变量
    }
  } else if (strstr((const char *)g_pucRX2Buffer, "VER") ||
             strstr((const char *)g_pucRX2Buffer,
                    "ver")) // VER指令，版本查询指令
  {

    VerHandleWBU();
    memset(ver_TXBuffer, '\0', sizeof(ver_TXBuffer));
    strcpy((char *)ver_TXBuffer, (char const *)pVerChr);
    Send_len = strlen((char *)ver_TXBuffer);
    if (Send_len > 512)
      Send_len = 512;
    if (Send_len > 0)
      UARTSend(2, ver_TXBuffer, Send_len);
  }

  else if (strstr((const char *)g_pucRX2Buffer, "ERR") ||
           strstr((const char *)g_pucRX2Buffer, "err")) // ERR指令，报警查询指令
  {
    SendErrorStr();        // 处理报警信息
    if (ALARM_OK == false) // 如果没有报警发生，则返回NO ALARM
    {
      UARTSend(2, NoalarmStr, 10);
    } else if (strlen(buffer_SendCommandErr) >
               2) // 否则返回报警信息内容buffer_SendCommandErr
      UARTSend(2, (uint8_t const *)buffer_SendCommandErr,
               strlen(buffer_SendCommandErr));
    ALARM_OK = false;
  } else if (strstr((const char *)g_pucRX2Buffer, "RDP") ||
             strstr((const char *)g_pucRX2Buffer,
                    "rdp")) // RDP指令，参数读取指令
  { // 接收rpd+空格+数据ID，例如rdp 100,读取数据ID是100的数值
    if (strstr((const char *)g_pucRX2Buffer, "RDP"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX2Buffer, "RDP");
    else if (strstr((const char *)g_pucRX0Buffer, "rdp"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX2Buffer, "rdp");
    memset(rdp_TXBuffer, '\0', sizeof(rdp_TXBuffer));
    p = (pRX0Buffer + 4);                       // 接收数组第5个缓存起是数据ID
    RdpIndex = (uint16_t)atoi((char const *)p); // 获取数据ID
    if (RdpIndex >= 100 &&
        RdpIndex <=
            115) { // 回复[数据ID]=数值，例如数据ID是100的数值为200，返回格式为[100]=200,
      strcpy((char *)rdp_TXBuffer, "[");
      Dec_ASCII_BCD(RdpIndex, 0, DecValASCII);
      strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
      strcat((char *)rdp_TXBuffer, "]=");
      Dec_ASCII_BCD(PST_data[RdpIndex], 0, DecValASCII);
      strcat((char *)rdp_TXBuffer, (const char *)DecValASCII);
    }
    strcat((char *)rdp_TXBuffer, "\r\n");
    Send_len = strlen((char *)rdp_TXBuffer);
    if (Send_len > 50)
      Send_len = 50;
    if (Send_len > 0)
      UARTSend(2, rdp_TXBuffer, Send_len);
  } else if (strstr((const char *)g_pucRX2Buffer, "WRP") ||
             strstr((const char *)g_pucRX2Buffer,
                    "wrp")) // WRP指令，参数写入指令，上位机写到控制器
  { // 接收到的指令格式为wrp+空格+数据ID+空格+数值，例如WRP 100
    // 120,修改数据ID是100的数值为120
    SWCommdIDWRP = 0; // 参数地址（modbus RTU 数据寄存器地址)
    SWCommdValue = 0; // 参数值（modbus RTU 数据寄存器数值)
    if (strstr((const char *)g_pucRX2Buffer, "WRP"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX2Buffer, "WRP");
    else if (strstr((const char *)g_pucRX2Buffer, "wrp"))
      pRX0Buffer = (char *)strstr((const char *)g_pucRX2Buffer, "wrp");
    p = (pRX0Buffer + 4);                           // 第5个字节起为数据ID
    SWCommdIDWRP = (uint16_t)atoi((char const *)p); // 获取数据ID
    Mbustemp[0] = 0x01;                             // Devie ID=1
    Mbustemp[1] = 0x06;                             // funtion	=6
    if (SWCommdIDWRP >= 100 && SWCommdIDWRP <= 115) // 数据ID必须在100~115范围内
    {
      if (SWCommdTime2 == 0) // 如果RS485端口是写入状态则可以写入
      {
        SWCommdValue = (uint16_t)atoi(
            (char const *)(pRX0Buffer +
                           8)); // 数据值为接收到的缓存的第9个字节起，获取数据值
        Mbustemp[2] = SWCommdIDWRP / 0x100; // 数据ID高16位
        Mbustemp[3] = SWCommdIDWRP % 0x100; // 数据ID低16位
        Mbustemp[4] = SWCommdValue / 0x100; // 数据值高16位
        Mbustemp[5] = SWCommdValue % 0x100; // 数据值低16位
        ModbusCRC(Mbustemp, 6);             // modbus crc校验
        Mbustemp[6] = g_lowCrcbyte;         // crc
        Mbustemp[7] = g_highCrcbyte;        // crc
        SWCommdTime2 = 1;                   // RS485写参数标记置1
        SWCommdTime2Times = 0;              // RS485写参数次数复位
        // Delay(2000000);//RS485延时2秒
        UARTSend(1, Mbustemp, 8); // RS485发送Modbus指令给控制器
        j = 0;                    // 复位RS485接收数据长度
        // ROM_TimerEnable(TIMER2_BASE, TIMER_A);//启动RS485定时器
      }
    }
  }
  memset(g_pucRX2Buffer, 0, sizeof(g_pucRX2Buffer));
  l = 0; // 复位Console接收数据
}

///***************************************************************************
void RS485IntHandler(void) // RS485接收中断调用函数
{
  uint8_t Send_len;
  uint8_t SW_TXBuffer[50] = {0};
  uint8_t DecValASCII[8];
  // 根据功能代码类型 写数据到对应寄存器地址
  ModbusCRC(g_pucRX1Buffer, j - 2);
  if ((g_pucRX1Buffer[j - 2] == g_lowCrcbyte) &&
      (g_pucRX1Buffer[j - 1] == g_highCrcbyte) &&
      (g_pucRX1Buffer[0] == 1)) // CRC校验和设备ID符合
  {
    // 接收Modbus功能代码3的数据
    if ((g_pucRX1Buffer[1] == 0x03) && (g_pucRX1Buffer[2] == j - 5) &&
        ModbusSDataLeth[ModbusCmdFlag] ==
            g_pucRX1Buffer[2] * 2) // 功能代码，数据长度符合
    {
      for (uint32_t len = 0; len < g_pucRX1Buffer[2] / 2; len++) {
        PST_data[ModbusStartAdd[ModbusCmdFlag] + len] =
            g_pucRX1Buffer[3 + len * 2] * 0x100 + g_pucRX1Buffer[4 + len * 2];
      }
      // SysSTAYear = PST_data[63];
      // if (SysSTAYear > 99)
      //   SysSTAYear = 99;
      // // SysSTAMonth=PST_data[62];
      // // SysSTADay=PST_data[61];
      // // SysSTAHour=PST_data[64];
      // // SysSTAMinute=PST_data[65];
      // // SysSTASecond=PST_data[66];
      // //
      // SysSTA=SysSTAYear*12*30*24*60*60+SysSTAMonth*30*24*60*60+SysSTADay*24*60*60+SysSTAHour*60*60+SysSTAMinute*60+SysSTASecond;//计算控制返回的系统时间按秒
      // if (SysStart == 0) // 协议转换器初次启动
      // {
      //   SysSTAPre = SysSTA;
      //   SysStart = 1;
      // }
      UnitType = PST_data[50]; // 机组型号是滴50个数值，数值为2等于WBU机组
      SendErrorGetOffStr();    // 定时发送报警信息给PC
    } else if (g_pucRX1Buffer[1] == 0x06 && j >= 8) // 功能代码6
    { // 接收到的功能代码6的modbus回应
      memset(SW_TXBuffer, 0, sizeof(SW_TXBuffer));
      if (SWCommdIDWRP == (g_pucRX1Buffer[2] * 0x100 + g_pucRX1Buffer[3]) &&
          SWCommdValue == (g_pucRX1Buffer[4] * 0x100 + g_pucRX1Buffer[5]) &&
          SWCommdIDWRP >= 100 &&
          SWCommdIDWRP <=
              115) { // 如果收到的modbus
                     // 的数据地址，数据值与RS485发送的一致，则表示写入正常，则RS232给尚未SVU返回正确的wrp回复[数据ID]=写入数值，例如[100]=120,数据ID为100的数值写入120
        strcpy((char *)SW_TXBuffer, "[");
        Dec_ASCII_BCD(SWCommdIDWRP, 0, DecValASCII);
        strcat((char *)SW_TXBuffer, (const char *)DecValASCII);
        strcat((char *)SW_TXBuffer, "]=");
        Dec_ASCII_BCD(SWCommdValue, 0, DecValASCII);
        strcat((char *)SW_TXBuffer, (const char *)DecValASCII);
        strcat((char *)SW_TXBuffer, "\r\n");
        SWCommdValue = 0;
        SWCommdIDWRP = 0;
        Send_len = strlen((char *)SW_TXBuffer);
        if (Send_len > 50)
          Send_len = 50;
        if (Send_len > 0)
          UARTSend(0, SW_TXBuffer, Send_len);
      }
      SWCommdTime2 = 0;
      SWCommdTime2Times = 0;
      memset(SW_TXBuffer, 0, sizeof(SW_TXBuffer));
    } // 如果没有收到正确modbus回复，则不响应RS232 WRP指令
  }
  memset(g_pucRX1Buffer, 0, 256);
  j = 0; // 清除RS454接收缓存
}

static void
SerialUARTIntHandler(uint32_t ulPort) // RS232/Console/RS485端口数据接收函数
{
  // uint32_t ulStatus;
  // uint8_t ucChar;
  // while(ROM_UARTCharsAvail(g_ulUARTBase[ulPort]))
  {
    // ucChar = ROM_UARTCharGet(g_ulUARTBase[ulPort]);
    if (ulPort == 0) // RS232端口
    {
      //    g_pucRX0Buffer[i++]= ucChar;
      if (i > 512)
        i = 0;
    }
    if (ulPort == 1) // RS485
    {
      //    g_pucRX1Buffer[j++]= ucChar;
      if (j > 512)
        j = 0;
    }
    if (ulPort == 2) // Console
    {
      //  g_pucRX2Buffer[l++]= ucChar;
      if (l > 512)
        l = 0;
    }
  }
}

//********************************************************************* */
void UARTSend(
    uint32_t uPort, const uint8_t *Buffer,
    uint32_t
        uclen) // 串口发送函数,
               // uPort为端口号，0为RS232，1为RS485，2为Console，Buffer为发送缓存，uclen发送缓存数据长度
{
  while (uclen--) {
    MAP_UARTCharPut(g_ulUARTBase[uPort], *Buffer++); // 从不同的端口发送
  }
}
//********************************************************************* */
void RS232RecIntHandler(void) // RS232接收中断
{
  SerialUARTIntHandler(0); // RS232接收数据
  if (UnitType == 2)       // 如果是WBU机组
  {
    RS232ManageHandlerWBU(); // 响应SUV上位机指令(STA,VER,EER,RDP.WRP)
  }
}

//********************************************************************* */
void RS485RecIntHandler(void) // RS485接收中断
{
  SerialUARTIntHandler(1); // RS485接收数据,标准的modbus RTU协议
  RS485IntHandler();       // RS485解析接收数据
}

//********************************************************************* */
void ConsoleRecIntHandler(void) // Console接收中断
{
  SerialUARTIntHandler(2); // Console接收数据
  if (UnitType == 2)       // 如果是WBU机组
  {
    ConsoleManageHandlerWBU(); // 响应PC上位机指令(STA,VER,EER,RDP.WRP)
  }
}

//************************************************************************** */
void RS485AIntHandler(
    void) // RS485A定时器调用函数（RS485定时器周期为500ms），定时发送读取控制器状态的2条指令。
{

  if (SWCommdTime2 == 0) // 如果RS485处于闲置状态则发送定时扫描指令
  {
    if (ModbusCmd >= 2)
      ModbusCmd = 0;
    ModbusCmdFlag = ModbusCmd;
    if (ModbusCmd == 0) {
      ModbusCRC(Temp_Command1, 6);
      Temp_Command1[6] = g_lowCrcbyte;
      Temp_Command1[7] = g_highCrcbyte;
      UARTSend(1, Temp_Command1, 8);
    }
    if (ModbusCmd == 1) {
      ModbusCRC(Temp_Command2, 6);
      Temp_Command2[6] = g_lowCrcbyte;
      Temp_Command2[7] = g_highCrcbyte;
      UARTSend(1, Temp_Command2, 8);
    }
    ModbusCmd++;
  } else if (SWCommdTime2Times < 1 &&
             SWCommdTime2 == 1) // 如果RS485是写参数状态，则再发送一次写参数指令
  {
    UARTSend(1, Mbustemp, 8);
    SWCommdTime2Times++;
  } else if (SWCommdTime2Times >=
             1) // 如果写入次数达到2次，则不再写入，置RS485为闲置状态
  {             // RS232或Console写过一次，RS485定时器再写一次，总共两次
    SWCommdTime2 = 0;
    SWCommdTime2Times = 0;
  }
}
