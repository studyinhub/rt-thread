#include "board.h"
#include "modbus_x.h"
#include "myConfig.h"
#include "agile_modbus.h"

// #include "mb.h"
// #include "mb_m.h"
// #include "user_mb_app.h"

#include "crc16.h"
#include "utils.h"
#include "board.h"

#define LOG_TAG "modbusx"
#define LOG_LVL LOG_LVL_DBG // LOG_LVL_DBG LOG_LVL_ERROR
#include <ulog.h>

#define SCAN_READ_BYTES 205

int RAddLimitMax = 255; // 允许读寻址的最大值
int RAddLimitMin = 0;   // 允许读寻址的最小值
int WAddLimitMax = 255; // 允许写寻址的最大值
int WAddLimitMin = 0;   // 允许写寻址的最小

extern struct rt_mailbox asc_resp_mb;
extern struct rt_messagequeue asc_send_mq;

// extern USHORT usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];

// 01 03 00 01 00 01 D5 CA
// 01 03 00 00 00 0A C5 CD

agile_modbus_rtu_t g_ctx_rtu;
agile_modbus_t *g_ctx = &g_ctx_rtu._ctx;

uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
uint8_t ctx_read_buf[SCAN_READ_BYTES]; // AGILE_MODBUS_MAX_ADU_LENGTH

// 互斥量，当在解析的过程中，禁止更新数据

int g_read_len;

// void send_rtu_frame(uint8_t slaveAddr, uint8_t funCode, uint16_t regStart, uint16_t regCnt, uint16_t bytesCnt, uint8_t *buf)
// {
//     char *pwBuf;
//     int i = 0;
//     uint16_t crc;

//     // log_i("发送功能码%d到%d", funCode, slaveAddr);

//     pwBuf = rt_malloc(24);
//     rt_memset(pwBuf, '\0', 24);
//     struct SER_PORT *port = &g_stConfig.serPort[0];

//     *pwBuf = slaveAddr;
//     i += 1;

//     *(pwBuf + i) = funCode;
//     i += 1;

//     *(pwBuf + i) = (regStart >> 8) && 0xFF;
//     i += 1;

//     *(pwBuf + i) = regStart & 0x00ff;
//     i += 1;

//     // log_d("读/写寄存器数:(%d)", regCnt);

//     *(pwBuf + i) = (regCnt >> 8) && 0xFF;
//     i += 1;

//     *(pwBuf + i) = regCnt & 0x00FF;
//     i += 1;

//     if (bytesCnt)
//     {
//         // 一个字节
//         // https://img-blog.csdnimg.cn/20190114112803875.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2xpYm94aXU=,size_16,color_FFFFFF,t_70
//         log_d("需要写入字节数:(%d)", bytesCnt);
//         *(pwBuf + i) = bytesCnt;
//         i += 1;
//     }

//     if (buf != NULL)
//     {
//         for (int j = 0; j < bytesCnt; j++)
//         {
//             log_d("i:%d j:%d %02X ", i, j, *(buf + j));
//             *(pwBuf + i + j) = *(buf + j);
//         }
//         i += 2;
//     }

//     crc = CRC(pwBuf, i);        // CRC校验，7+k*2是校验的长度
//     *(pwBuf + i) = crc / 0x100; // 校验值高8位
//     i += 1;

//     *(pwBuf + i) = crc % 0x100; // 校验值低8位
//     i += 1;

//     // log_d("发送到 RTU：crc:%04X", crc);
//     // for (int j = 0; j < i; j++)
//     // {
//     //     rt_kprintf("%02X ", *(pwBuf + j));
//     // }
//     // rt_kprintf("\n");

//     rt_device_write(port->device, 0, pwBuf, i);
//     free(pwBuf);
// }

// :011706000000000000E2

// :01 17 1A 0328 0000 0000 0010 0000 0000 0000 0000 0000 0000 0000 0000 0000 93\CR\LF
// void response_ascii_frame(uint8_t slaveAddr, uint8_t funCode, uint8_t *buf, uint16_t bufLen)
// {
//     char *pwBuf;
//     uint8_t calc_lrc = 0;
//     int i = 0;

//     // i = 1;
//     // unsigned char *pointer;
//     // pointer = (unsigned char *)&i;

//     // if (*pointer)
//     // {
//     //     log_w("litttle_endian");
//     // }
//     // else
//     // {
//     //     log_w("big endian/n");
//     // }

//     // for (i = 0; i < byteCnt; i++)
//     // {
//     //     log_d("i:%d v:%02X:", i, *(buf + i));
//     // }

//     // :+（slaveAddr + funcode + quantity bytes + lrc） * 2 + datalen + '\r\n'
//     // 其中 datalen 已经是字节数了

//     // uint16_t frame_len = 1 + (1 + 1 + 1 + 1 + byteCnt) * 2 + 2;
//     // log_d("frame_len:%d", frame_len);

//     // pwBuf = rt_malloc(frame_len);
//     // rt_memset(pwBuf, '\0', frame_len);

//     // *pwBuf = ':';
//     // HToAChar(pwBuf + 1, &slaveAddr, 1);
//     // HToAChar(pwBuf + 3, &funCode, 1);
//     // HToAChar(pwBuf + 5, (uint8_t *)&byteCnt, 1);
//     // HToAChar(pwBuf + 7, (uint8_t *)buf, byteCnt);

//     // // // 补充上 LRC
//     // calc_lrc = ASCII_LRC(pwBuf + 1, 2 * (3 + byteCnt)); //

//     // log_d("calc_lrc:0x%02X", calc_lrc);

//     // HToAChar(pwBuf + 7 + 2 * byteCnt, (uint8_t *)&calc_lrc, 1);
//     // rt_memcpy(pwBuf + 7 + 2 * byteCnt + 2, "\r\n", 2);

//     // // for (int j = 0; j < frame_len; j++)
//     // // {
//     // //     rt_kprintf("%02x ", *(pwBuf + j));
//     // // }

//     // // log_d("res:%s", pwBuf);

//     // for (int i = 0; i < frame_len; i++)
//     // {
//     //     rt_kprintf("%c", *(pwBuf + i));
//     // }

//     // struct SER_PORT *port = &g_stConfig.serPort[1];
//     // log_d("port:%s", port->dev_name);
//     // rt_device_write(port->device, 0, pwBuf, frame_len);
//     // free(pwBuf);
// }

// void read_regs(uint8_t slaveAddr, uint8_t funCode, uint16_t regStart, uint16_t regCnt)
// {
//     switch (funCode)
//     {
//     case 3:
//         // 01 03 00 00 00 0D 00 00 23 04
//         // send_rtu_frame(slaveAddr, funCode, regStart, regCnt, 0, NULL);
//         break;
//     case 1:
//     case 2:
//     case 4:
//         log_e("not suppored");
//         break;
//     default:
//         break;
//     }
// }

// void write_regs(uint8_t slaveAddr, uint8_t funCode, uint16_t regStart, uint16_t regCnt, uint16_t bytesCnt, uint8_t *buf)
// {
//     log_d("准备写数据");
//     switch (funCode)
//     {
//     case 5:
//     case 6:
//     case 15:
//         log_e("not suppored");
//         break;
//     case 16:
//         // 01 10 00 0C 00 01 02 00 01 67 5C
//         // 01 10 00 0C 00 01 02 00 00 A6 9C
//         // send_rtu_frame(slaveAddr, funCode, regStart, regCnt, bytesCnt, buf);
//         break;
//     default:
//         break;
//     }
// }

int rtu_read_holdings(int regStartAddr, int regCnt, uint16_t *holdBuf)
{
    int send_len = 0, read_len = 0, rc = 0, ret = 0;

    uint8_t temp[SCAN_READ_BYTES];

    // log_d("regStartAddr:%d", regStartAddr);
    // log_d("regCnt:%d", regCnt);
    // log_d("holdBuf:%d", holdBuf);

    rt_memset(g_ctx->send_buf, 0, g_ctx->send_bufsz);
    send_len = agile_modbus_serialize_read_registers(g_ctx, regStartAddr, regCnt);
    // log_d("send_len:%d", send_len);

    rs485_send(g_stConfig.serPort[0].device, g_ctx->send_buf, send_len);
    // ulog_hexdump("send", 16, g_ctx->send_buf, send_len);
    // g_stConfig.serPort[0].frameInterval
    rt_memset(g_ctx->read_buf, 0, g_ctx->read_bufsz);
    // 读取数据,500 ms 内读完
    // 帧间隔:g_stConfig.serPort[0].frameInterval
    read_len = rs485_receive(g_ctx->read_buf, g_ctx->read_bufsz, 500);

    if (read_len <= 0 || read_len != g_ctx->read_bufsz)
    {
        // log_d("数据读取错误 readsize:%d read_len:%d",g_ctx->read_bufsz,read_len);
        // 抽干串口端的数据
        rs485_receive(temp, SCAN_READ_BYTES, 500);
        rt_memset(temp, '\0', g_ctx->read_bufsz);
        // log_e("没有读取到数据,检查下位机设备是否连接正常");
        // rt_pin_write(LED_WRK, !(rt_pin_read(LED_WRK)));
        easyblink(led_wrk, -1, 500, 500);
        return -1;
    }

    // ulog_hexdump("recv", 16, g_ctx->read_buf, g_ctx->read_bufsz);

    // 解析读到的数据g_stConfig.scanTask[i].hold g_hold_register
    rc = agile_modbus_deserialize_read_registers(g_ctx, read_len, holdBuf);

    if (rc < 0)
    {
        log_e("Receive failed.%d", rc);
        if (rc != -1)
            LOG_W("Error code:%d", -128 - rc);
    }
    else
    {
        easyblink_stop(led_wrk);
        eb_led_on(led_wrk);
    }

    return ret;
}

// 解析收到的 ascii 帧
uint8_t ascii_parse(char *ptrFrame, rt_uint32_t frame_len)
{

    uint8_t ret = 0;

    clock_t start = 0, end = 0;
    start = clock();
    // log_d("开始解析 ASCII frame:%d", frame_len);

    uint8_t asc_slaveAddr = 1; // 上位机请求中的从机地
    uint8_t asc_funcode = 0;   // 上位机请求中的从机地址

    uint8_t rtu_funCode = 0;

    uint16_t ReadDatStAdd = 0; // 上位机要的读的数据起始
    int ReadDatLenth = 0;      // 上位机要读的数据的长度

    uint16_t WriteSingleData = 0; // 06 功能码写单个寄存器的数据

    uint16_t WriteDatStAdd = 0; // 上位机要写的数据的起始地址
    int WriteDatLenth = 0;      // 上位机要写的数据的长度
    int WriteByteNum = 0;       // 上位机要写的数据的字节长度=数据长度*2

    uint8_t lrc = 0; // 上位机请求中的 LRC

    rt_bool_t isValid = RT_FALSE;

    uint8_t *pwBuf = RT_NULL;
    char *pAscWbuf = RT_NULL;

    int send_len = 0;

    int i = 0, j = 0;

    uint16_t bufLen = 0;
    uint16_t bytesCnt = 0;       // 3 号功能码回复才有
    uint16_t wrRegStartAddr = 0; // 16 号功能码回复才有 写入寄存器起始地址
    uint16_t wrRegCnt = 0;       // 16 号功能码回复才有 写入寄存器数量
    uint8_t rdRegCnt = 0;        // 23 号功能回复才有，读取结存器数量
    uint8_t calc_lrc = 0;
    uint16_t calr_lrc_len = 0;

    uint16_t rtu_wr_buf[AGILE_MODBUS_MAX_WRITE_REGISTERS];

    int rc = 0;

    log_d("ascii_parse");
    if (LOG_LVL == LOG_LVL_DBG)
    {
        for (i = 0; i < frame_len; i++)
        {
            rt_kprintf("%c", *(ptrFrame + i));
        }
    }

    for (i = 0, j = 0; i < frame_len; i++)
    {
        // 查找帧头
        if (*(ptrFrame + i) != ':')
        {
            continue;
        }
        j += 1;
        asc_slaveAddr = ATOHChar(ptrFrame + i + j);
        j += 2;
        log_d("asc_slaveAddr:%d", asc_slaveAddr);

        // if (asc_slaveAddr != g_stConfig.ascAddr)
        // {
        //     LOG_W("Please check the slave addr");
        //     break;
        // }

        asc_funcode = ATOHChar(ptrFrame + i + j);
        j += 2;
        log_d("funcode:0x%02x", asc_funcode);

        // if (funcode != 0x17) //功能代码符合0x17
        // {
        //     LOG_W("Only support 0x17 but got 0x%02x", asc_funcode);
        //     continue;
        // }

        if (asc_funcode == 0x03)
        {
            ReadDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            // log_d("ReadDatStAdd:%04x", ReadDatStAdd);

            // 功能代码23（0x17)要读取的数据起始地址，占4个字节[3]~[6]
            ReadDatLenth = ATOHInt(ptrFrame + i + j);
            j += 4;
        }

        else if (asc_funcode == 0x06)
        {
            WriteDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            // log_d("WriteDatStAdd:%04x", WriteDatStAdd); //功能代码23（0x17)要写取的数据起始地址，占4个字节[11]~[14]

            // WriteSingleData = ATOHInt(ptrFrame + i + j);

            // j += 4;
            // log_d("WriteSingleData:%04x", WriteSingleData); //功能代码23（0x17)要写取的数据起始地址，占4个字节[11]~[14]

            WriteDatLenth = 1;
            WriteByteNum = 2;

            pAscWbuf = rt_malloc(WriteByteNum);
            rt_memset(pAscWbuf, '\0', WriteByteNum);
            for (int k = 0; k < WriteByteNum; k++)
            {
                int temp = ATOHChar(ptrFrame + i + j + 2 * k);
                // rt_kprintf("temp:%02x ", temp);
                *(pAscWbuf + k) = temp;
                // rt_kprintf("pAscWbuf:%02x ", *(pAscWbuf + k));
                // if (k == WriteByteNum)
                //     rt_kprintf("\r\n");
            }
            j += WriteByteNum * 2; //  注意这里是 * 2 ，不是 *4，长度 4 个字节，ASCII 的表示是翻倍的，占用 4*2 个 字节。
        }
        else if (asc_funcode == 0x10)
        {

            WriteDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            log_d("WriteDatStAdd:%04x", WriteDatStAdd); // 功能代码16（0x10)要写取的数据起始地址，占4个字节[11]~[14]

            WriteDatLenth = ATOHInt(ptrFrame + i + j); // 功能代码16（0x10)要写取的寄存器数量，占4个字节[15]~[18]
            j += 4;
            log_d("WriteDatLenth:%04x", WriteDatLenth);

            WriteByteNum = ATOHChar(ptrFrame + i + j); // 功能代码16（0x10)要写取的数据字节个数，占2个字节[19]~[20]
            j += 2;
            log_d("WriteByteNum:%02x", WriteByteNum);

            // if (!(WriteByteNum == WriteDatLenth * 2 && (WriteDatLenth == 0 || (WriteDatLenth > 0 && WriteDatStAdd >= WAddLimitMin && WriteDatStAdd <= WAddLimitMax && WriteDatStAdd + WriteDatLenth >= WAddLimitMin + 1 && WriteDatStAdd + WriteDatLenth <= WAddLimitMax + 1)))) // 写的地址和长度符合
            // {
            //     // 写的数据的起始地址不能超范围，写的数据起始地址+数据长度不能超范围
            //     LOG_W("Invalid write parameters");
            //     ret = -1;
            //     goto exit;
            // }

            pAscWbuf = rt_malloc(WriteByteNum);
            rt_memset(pAscWbuf, '\0', WriteByteNum);
            for (int k = 0; k < WriteByteNum; k++)
            {
                int temp = ATOHChar(ptrFrame + i + j + 2 * k);
                // rt_kprintf("temp:%02x ", temp);
                *(pAscWbuf + k) = temp;
                // rt_kprintf("pAscWbuf:%02x ", *(pAscWbuf + k));
                // if (k == WriteByteNum)
                //     rt_kprintf("\r\n");
            }
            j += WriteByteNum * 2; //  注意这里是 * 2 ，不是 *4，长度 4 个字节，ASCII 的表示是翻倍的，占用 4*2 个 字节。
        }

        else if (asc_funcode == 0x17)
        {
            ReadDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            // log_d("ReadDatStAdd:%04x", ReadDatStAdd);

            // 功能代码23（0x17)要读取的数据起始地址，占4个字节[3]~[6]
            ReadDatLenth = ATOHInt(ptrFrame + i + j);
            j += 4;
            // log_d("ReadDatLenth:%04x", ReadDatLenth);

            // 功能代码23（0x17)要读取的数据长度，占4个字节[7]~[10]

            if (!g_stConfig.mapEnable)
            {
                if (!(ReadDatStAdd >= RAddLimitMin && ReadDatStAdd <= RAddLimitMax && ReadDatStAdd + ReadDatLenth >= RAddLimitMin && ReadDatStAdd + ReadDatLenth <= RAddLimitMax + 1)) // 读的地址和长度符合
                // 起始地址不能超范围，起始地址+数据长度不能超范围
                {
                    LOG_W("Invalid read parameters");
                    break;
                }
            }
            else
            {
                log_d("启用了地址映射");
            }

            WriteDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            log_d("WriteDatStAdd:%04x", WriteDatStAdd); // 功能代码23（0x17)要写取的数据起始地址，占4个字节[11]~[14]

            WriteDatLenth = ATOHInt(ptrFrame + i + j); // 功能代码23（0x17)要写取的寄存器数量，占4个字节[15]~[18]
            j += 4;
            log_d("WriteDatLenth:%04x", WriteDatLenth);

            WriteByteNum = ATOHChar(ptrFrame + i + j); // 功能代码23（0x17)要写取的数据字节个数，占2个字节[19]~[20]
            j += 2;
            log_d("WriteByteNum:%02x", WriteByteNum);

            // if (!(WriteByteNum == WriteDatLenth * 2 && (WriteDatLenth == 0 || (WriteDatLenth > 0 && WriteDatStAdd >= WAddLimitMin && WriteDatStAdd <= WAddLimitMax && WriteDatStAdd + WriteDatLenth >= WAddLimitMin + 1 && WriteDatStAdd + WriteDatLenth <= WAddLimitMax + 1)))) // 写的地址和长度符合
            // {
            //     // 写的数据的起始地址不能超范围，写的数据起始地址+数据长度不能超范围
            //     LOG_W("Invalid write parameters");
            //     ret = -1;
            //     goto exit;
            // }

            pAscWbuf = rt_malloc(WriteByteNum);
            rt_memset(pAscWbuf, '\0', WriteByteNum);
            for (int k = 0; k < WriteByteNum; k++)
            {
                int temp = ATOHChar(ptrFrame + i + j + 2 * k);
                // rt_kprintf("temp:%02x ", temp);
                *(pAscWbuf + k) = temp;
                // rt_kprintf("pAscWbuf:%02x ", *(pAscWbuf + k));
                // if (k == WriteByteNum)
                //     rt_kprintf("\r\n");
            }
            j += WriteByteNum * 2; //  注意这里是 * 2 ，不是 *4，长度 4 个字节，ASCII 的表示是翻倍的，占用 4*2 个 字节。
        }
        else
        {
            log_w("Unknow funcode");
        }

        calc_lrc = ASCII_LRC(ptrFrame + 1, j - 1); // j-1 表示不计算 ：
        // log_d("calc_lrc:%02X %d %d", calc_lrc, i, j);
        // for(int n = 0;n<j+2;n++)
        // {
        //     log_d("lrc %d:%02x",n,*(ptrFrame + i + n));
        // }

        lrc = ATOHChar(ptrFrame + i + j);
        log_d("lrc:%02X calc_lrc:%02X", lrc, calc_lrc);

        j += 2;
        if (calc_lrc != lrc || calc_lrc == -1) // LRC校验和判断 -1 表示计算失败
        {
            LOG_W("LRC mismatch!");
            break;
        }

        if (!(*(ptrFrame + i + j) == 0x0D && *(ptrFrame + i + j + 1) == 0x0A)) // 结束符0x0D,和0x0A符合
        {
            LOG_W("Not found 0x0D && 0x0A");
            break;
        }
        isValid = RT_TRUE;
        break;
    }

    log_w("i:%d frame_len:%d", i, frame_len);

    if (i == frame_len)
    {
        log_e("未找到帧头%d %d", i, frame_len);
        for (i = 0; i < frame_len; i++)
        {
            rt_kprintf("%c", *(ptrFrame + i));
        }
        ret = -1;
        goto exit;
    }

    if (!isValid)
    {
        log_e("解析失败%d %d", i, frame_len);

        // 解析失败，将邮件扔掉
        struct SER_PORT *resp_port;
        if (rt_mb_recv(&asc_resp_mb, (rt_ubase_t *)&resp_port, 1 * RT_TICK_PER_SECOND) == RT_EOK)
        {
            log_d("resp_port->dev_name:%s", resp_port->dev_name);
            // rt_device_write(resp_port->device, 0, pwBuf, bufLen);
        }

        ret = -1;
        goto exit;
    }

    // NO  Code                         Name                                 Function
    // 1    03(03h)             Read holding registers                Reading multiple registers
    // 2    06(06h)             Preset single register                Writing registers
    // 3    16(10h)             Preset multiple registers             Writing multiple registers
    // 4    23(17h)             Read/write 4x registers/              Reading/writing multiple registers

    end = clock();
    log_d("解析完毕,用时:%dms", end - start);

    switch (asc_funcode)
    {
    case 03:
        log_d("从机地址:%d", asc_slaveAddr);
        log_d("功能码:%d Reading multiple registers", asc_funcode);
        log_d("读起始地址:%d", ReadDatStAdd);
        log_d("读寄存器数量:%d", ReadDatLenth);
        if (ReadDatStAdd)
        {
            log_d("ReadDatStAdd:%d,%d", ReadDatStAdd, ReadDatLenth);
        }
        rtu_funCode = *(g_ctx->read_buf + 1);
        log_d("rtu_funCode:%d", rtu_funCode);

        bytesCnt = ReadDatLenth * 2;

        // pwBuf 长度:是整个 frame 的长度，避免再次分配内存，这里一次分配
        // 1(:)+(slaveAddr + funCode + bytesCnt + bytes + lrc)*2 +  \r\n
        bufLen = 1 + (1 + 1 + 1 + bytesCnt + 1) * 2 + 2;
        calr_lrc_len = bufLen - 5;
        // log_w("bufLen:%d",bufLen);
        // 多分配 1 个字节，用于保存 '\0'
        pwBuf = rt_malloc(bufLen + 1);
        rt_memset(pwBuf, '\0', bufLen + 1);
        // 写入 bytesCnt
        *pwBuf = ':';
        HToAChar(pwBuf + 1, &asc_slaveAddr, 1, 0);

        rtu_funCode = 03;

        HToAChar(pwBuf + 3, &rtu_funCode, 1, 0);

        log_d("bytesCnt:%d", bytesCnt);

        HToAChar(pwBuf + 5, (uint8_t *)&bytesCnt, 1, 0);
        // fixed：+2*ReadDatStAdd
        HToAChar(pwBuf + 7, g_ctx->read_buf + 3 + 2 * ReadDatStAdd, bytesCnt, 0);
        // log_d("功能码之后 LRC 之前的字符:%s",pwBuf);
        // LRC 不包括 : \r\n
        // 2 * (3 + bytesCnt) == bufLen-5

        calc_lrc = ASCII_LRC(pwBuf + 1, calr_lrc_len);
        log_d("calc_lrc:0x%02X", calc_lrc);

        // 倒数第4个起
        HToAChar(pwBuf + bufLen - 4, (uint8_t *)&calc_lrc, 1, 0);
        rt_memcpy(pwBuf + bufLen - 2, "\r\n", 2);
        *(pwBuf + bufLen) = '\0';

        break;
    case 06:
        log_d("从机地址:%d", asc_slaveAddr);
        log_d("功能码:%d Writing register", asc_funcode);
        log_d("写地址:%04x", WriteDatStAdd);

        log_d("*(pAscWbuf + 2 * x):%d", *(pAscWbuf));
        log_d("*(pAscWbuf + 2 * x+1):%d", *(pAscWbuf + 1));

        rtu_wr_buf[0] = (*(pAscWbuf) << 8) + *(pAscWbuf + 1);
        log_d("rtu_wr_buf[0]:%d", rtu_wr_buf[0]);

        send_len = agile_modbus_serialize_write_register(g_ctx, WriteDatStAdd, (const uint16_t)rtu_wr_buf[0]);
        // send_len = agile_modbus_serialize_write_registers(g_ctx, WriteDatStAdd, WriteDatLenth, (const uint16_t *)&rtu_wr_buf);
        rs485_send(g_stConfig.serPort[0].device, g_ctx->send_buf, send_len);

        char *retBuf = rt_malloc(g_ctx->read_bufsz);
        rt_memset(retBuf, '\0', g_ctx->read_bufsz);
        g_read_len = rs485_receive(retBuf, g_ctx->read_bufsz, 1000);

        log_d("g_read_len:%d", g_read_len);
        if (g_read_len)
            log_hex("rtu ret:", 16, retBuf, g_read_len);

        rtu_funCode = *(retBuf + 1);

        // pwBuf 长度:是整个 frame 的长度，避免再次分配内存，这里一次分配
        // 1(:)+(slaveAddr + funCode + regAddress + bytes + lrc)*2 +  \r\n
        bufLen = 1 + (1 + 1 + 2 + 2 + 1) * 2 + 2;
        calr_lrc_len = bufLen - 5;
        // log_w("bufLen:%d",bufLen);
        // 多分配 1 个字节，用于保存 '\0'
        pwBuf = rt_malloc(bufLen + 1);
        rt_memset(pwBuf, '\0', bufLen + 1);

        // :0106000C0001EC
        *pwBuf = ':';
        HToAChar(pwBuf + 1, &asc_slaveAddr, 1, 0);

        rtu_funCode = 06;

        HToAChar(pwBuf + 3, &rtu_funCode, 1, 0);

        log_d("WriteDatStAdd:%04x", WriteDatStAdd);

        uint16_t convertAddr;
        convertAddr = ((WriteDatStAdd & 0x00ff) << 8) + ((WriteDatStAdd & 0xff00) >> 8);

        log_d("convertAddr:%04x", convertAddr);

        HToAChar(pwBuf + 5, (uint8_t *)&convertAddr, 2, 0);

        HToAChar(pwBuf + 9, retBuf + 4, 2, 0);

        calc_lrc = ASCII_LRC(pwBuf + 1, calr_lrc_len);
        // log_d("calc_lrc:0x%02X", calc_lrc);

        // // 倒数第4个起
        HToAChar(pwBuf + bufLen - 4, (uint8_t *)&calc_lrc, 1, 0);
        rt_memcpy(pwBuf + bufLen - 2, "\r\n", 2);
        *(pwBuf + bufLen) = '\0';

        rt_free(retBuf);
        break;

    case 16:
        log_d("从机地址:%d", asc_slaveAddr);
        log_d("功能码:%d writing multiple registers", asc_funcode);
        log_d("写起始地址:%d", WriteDatStAdd);
        log_d("写寄存器数量:%d", WriteDatLenth);
        log_d("写字节长度:%d", WriteByteNum);

        if (WriteDatLenth)
        {
            // if (ReadDatLenth)
            //     rt_thread_delay(100);

            log_d("准备写入数据: wsa:%d,wdl:%d,wbc:%d,wd:%d", WriteDatStAdd, WriteDatLenth, WriteByteNum, *pAscWbuf);

            for (int x = 0; x < WriteDatLenth; x++)
            {
                log_d("*(pAscWbuf + 2 * x):%d", *(pAscWbuf + 2 * x));
                log_d("*(pAscWbuf + 2 * x+1):%d", *(pAscWbuf + 2 * x + 1));
                rtu_wr_buf[x] = (*(pAscWbuf + 2 * x) << 8) + *(pAscWbuf + 2 * x + 1);
            }

            log_d("rtu_wr_buf[0]:%d", rtu_wr_buf[0]);

            send_len = agile_modbus_serialize_write_registers(g_ctx, WriteDatStAdd, WriteDatLenth, (const uint16_t *)&rtu_wr_buf);
            rs485_send(g_stConfig.serPort[0].device, g_ctx->send_buf, send_len);

            // char *retBuf = rt_malloc(g_ctx->read_bufsz);
            // rt_memset(retBuf, '\0', g_ctx->read_bufsz);
            // g_read_len = rs485_receive(retBuf, g_ctx->read_bufsz, 1000);

            log_d("g_read_len:%d", g_read_len);
            if (g_read_len)
                log_hex("rtu ret:", 16, retBuf, g_read_len);

            rtu_funCode = *(retBuf + 1);

            bufLen = 1 + (1 + 1 + 2 + 2 + 1) * 2 + 2;
            calr_lrc_len = bufLen - 5;
            // log_w("bufLen:%d",bufLen);
            // 多分配 1 个字节，用于保存 '\0'
            pwBuf = rt_malloc(bufLen + 1);
            rt_memset(pwBuf, '\0', bufLen + 1);

            // :0106000C0001EC
            *pwBuf = ':';
            HToAChar(pwBuf + 1, &asc_slaveAddr, 1, 0);

            HToAChar(pwBuf + 3, &rtu_funCode, 1, 0);

            log_d("WriteDatStAdd:%04x", WriteDatStAdd);

            uint16_t convertAddr;
            convertAddr = ((WriteDatStAdd & 0x00ff) << 8) + ((WriteDatStAdd & 0xff00) >> 8);

            log_d("convertAddr:%04x", convertAddr);

            HToAChar(pwBuf + 5, (uint8_t *)&convertAddr, 2, 0);

            HToAChar(pwBuf + 9, retBuf + 4, 2, 0);

            calc_lrc = ASCII_LRC(pwBuf + 1, calr_lrc_len);
            // log_d("calc_lrc:0x%02X", calc_lrc);

            // // 倒数第4个起
            HToAChar(pwBuf + bufLen - 4, (uint8_t *)&calc_lrc, 1, 0);
            rt_memcpy(pwBuf + bufLen - 2, "\r\n", 2);
            *(pwBuf + bufLen) = '\0';

            rt_free(retBuf);
        }

        break;

    case 23:
        log_d("从机地址:%d", asc_slaveAddr);
        log_d("功能码:%d Reading/writing multiple registers", asc_funcode);
        log_d("读起始地址:%d", ReadDatStAdd);
        log_d("读寄存器数量:%d", ReadDatLenth);
        log_d("写起始地址:%d", WriteDatStAdd);
        log_d("写寄存器数量:%d", WriteDatLenth);
        log_d("写字节长度:%d", WriteByteNum);

        // https://www.cnblogs.com/iluzhiyong/p/4929165.html
        // https://blog.csdn.net/liboxiu/article/details/86473516
        // https://www.cnblogs.com/wt88/p/9624373.html

        // 设备地址          Modbus地址                          描述                    功能                R/W
        // 1~10000         address-1        0xxxx           Coils（Output）          01,05,15              R/W
        // 10001~20000     address-10001    0xxxx           Discrete Inputs           02                  R
        // 30001~40000     address-30001    0xxxx           Input Registers           04                  R
        // 40001~50000     address-40001    0xxxx           Holding Registers         03                 R/W

        // 0x01: 读线圈寄存器          Coils                     R
        // 0x02: 读离散输入寄存器      Discrete Inputs            R
        // 0x03: 读保持寄存器         Holding Registers          R
        // 0x04: 读输入寄存器         Input Registers            R
        // 0x05: 写单个线圈寄存器                                 W
        // 0x06: 写单个保持寄存器                                 W
        // 0x0f: 写多个线圈寄存器                                 W
        // 0x10: 写多个保持寄存器                                 W

        if (WriteDatLenth)
        {
            // if (ReadDatLenth)
            //     rt_thread_delay(100);

            log_d("准备写入数据: wsa:%d,wdl:%d,wbc:%d,wd:%d", WriteDatStAdd, WriteDatLenth, WriteByteNum, *pAscWbuf);

            for (int x = 0; x < WriteDatLenth; x++)
            {
                log_d("*(pAscWbuf + 2 * x):%d", *(pAscWbuf + 2 * x));
                log_d("*(pAscWbuf + 2 * x+1):%d", *(pAscWbuf + 2 * x + 1));
                rtu_wr_buf[x] = (*(pAscWbuf + 2 * x) << 8) + *(pAscWbuf + 2 * x + 1);
            }

            log_d("rtu_wr_buf[0]:%d", rtu_wr_buf[0]);

            if (WriteDatStAdd >= 256)
            {
                WriteDatStAdd = WriteDatStAdd - 256 + g_stConfig.ascSys[asc_slaveAddr - 1].offset;
            }

            rt_memset(g_ctx->send_buf, 0, g_ctx->send_bufsz);
            send_len = agile_modbus_serialize_write_registers(g_ctx, WriteDatStAdd, WriteDatLenth, (const uint16_t *)&rtu_wr_buf);
            rs485_send(g_stConfig.serPort[0].device, g_ctx->send_buf, send_len);
            // char *retBuf = rt_malloc(g_ctx->read_bufsz);
            // rt_memset(retBuf, '\0', g_ctx->read_bufsz);
            // rt_memset(g_ctx->read_buf, '\0', g_ctx->read_bufsz);                 // g_ctx->read_bufsz ->8
            g_read_len = rs485_receive(g_ctx->read_buf, g_ctx->read_bufsz, 100); // g_ctx->read_bufsz -> 8 个字节
            // rt_kprintf("g_read_len:%d\r\n", g_read_len);
            // if (g_read_len)
            //     log_hex("rtu ret:", 16, g_ctx->read_buf, g_read_len);

            // rtu_funCode = *(retBuf + 1);

            // 写入结果 读取 并判断是否需要重写

            // rt_free(retBuf);

            // write_regs(g_stConfig.ascAddr, 16, WriteDatStAdd, WriteDatLenth, WriteByteNum, pAscWbuf);
        }

        if (ReadDatLenth)
        {
            // log_d("ReadDatStAdd:%d,%d,%d,%d", ReadDatStAdd, ReadDatLenth,g_stConfig.scanRegCnt,g_hold_register);
            // 根据 asc_slaveAddr 获取 scanTask

            // rt_mutex_take(dynamic_mutex, RT_WAITING_FOREVER);

            // 如果起始地址超过了扫描寄存器的数量 或者读数据长度超过了扫描寄存器的数量  或者 关闭了扫描功能，那么就直接不能从缓存读取了
            // ReadDatStAdd > g_stConfig.scanRegCnt || ReadDatLenth > g_stConfig.scanRegCnt

            // rtu_read_holdings(asc_slaveAddr,ReadDatStAdd,100,g_hold_register);

            // log_hex("rtu frame", 16, g_ctx->read_buf, g_read_len);
            // 不需要再去校验，agile_modbus_deserialize_read_registers 里边已经有校验
            // uint16_t rtu_recv_crc = 0;
            // uint16_t rtu_calc_crc = 0;

            // rtu_recv_crc = *(g_ctx->read_buf + g_read_len - 2) << 8 | *(g_ctx->read_buf + g_read_len - 1);
            // rtu_calc_crc = CRC(g_ctx->read_buf, g_read_len - 2);

            // log_d("rtu_recv_crc:%04X", rtu_recv_crc);
            // log_d("rtu_calc_crc:%04X", rtu_calc_crc);
            // if (rtu_recv_crc != rtu_calc_crc)
            // {
            //     log_e("rtu_recv_crc ne rtu_calc_crc");
            //     rt_memset(g_ctx->read_buf, 0, AGILE_MODBUS_MAX_ADU_LENGTH);
            //     return -1;
            // }

            // uint8_t rtu_slaveAddr = *(g_ctx->read_buf);
            // log_d("rtu_slaveAddr:%d %d", rtu_slaveAddr,g_ctx->slave);

            rtu_funCode = *(g_ctx->read_buf + 1);
            log_d("rtu_funCode:%d", rtu_funCode);

            // rt_mutex_release(dynamic_mutex);
            //  RT_TICK_PER_SECOND/100
        }

        // 如果要回复 16 号指令，那么需要另行考虑
        // wrRegStartAddr = *(ptrFrame + 2) << 8 | *(ptrFrame + 3);
        // wrRegCnt = *(ptrFrame + 4) << 8 | *(ptrFrame + 5);

        // 如果强制要求既读又写的回复定以为 :011700E8
        // 01 10 00 0C 00 01 C1 CA
        // 如果 23 号指令只写不读，那么固定回复如下
        // : 01 17 00 E8 \CR\LF
        // pwBuf 长度:是整个 frame 的长度，避免再次分配内存，这里一次分配
        // 1(:)+(slaveAddr + rtu_funCode + bytesReadCnts+lrc)*2

        if (0)
        {
            bufLen = 1 + (1 + 1 + 1 + 1) * 2 + 2;
            calr_lrc_len = bufLen - 5;
            log_w("bufLen:%d", bufLen);
            pwBuf = rt_malloc(bufLen + 1);
            rt_memset(pwBuf, '\0', bufLen + 1);
            *pwBuf = ':';
            HToAChar(pwBuf + 1, &asc_slaveAddr, 1, 0);

            rtu_funCode = 23; // 强制将 03 号回复为 23 号功能码
            HToAChar(pwBuf + 3, &rtu_funCode, 1, 0);
            uint8_t temp = 0;
            HToAChar(pwBuf + 5, &temp, 1, 0);
        }

        // 这里不再考虑 RTU 的功能码，回复给上位机的只有 23 号功能码
        // switch (rtu_funCode)
        // {
        // case 3:

        //     break;

        // case 16:

        //     break;

        // default:
        //     log_e("Not support rtu_funCode:%d", rtu_funCode);
        //     ret = -1;
        //     goto exit;
        //     break;
        // }

        // 读回复
        // 01 03 1A 00 0C 00 22 00 38 00 4E 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 00 42 C8
        // : 01 17 1A 0328 0000 0000 0010 0000 0000 0000 0000 0000 0000 0000 0000 0000 93\CR\LF
        // : 01 17 1A 000C 0022 0038 004E 000C 000C 000C 000C 000C 000C 000C 000C 0001 B9

        bytesCnt = ReadDatLenth * 2;

        // pwBuf 长度:是整个 frame 的长度，避免再次分配内存，这里一次分配
        // 1(:)+(slaveAddr + funCode + bytesCnt + bytes + lrc)*2 +  \r\n
        bufLen = 1 + (1 + 1 + 1 + bytesCnt + 1) * 2 + 2;
        calr_lrc_len = bufLen - 5;
        // log_w("bufLen:%d",bufLen);
        // 多分配 1 个字节，用于保存 '\0'
        pwBuf = rt_malloc(bufLen + 1);
        rt_memset(pwBuf, '\0', bufLen + 1);
        // 写入 bytesCnt
        *pwBuf = ':';
        HToAChar(pwBuf + 1, &asc_slaveAddr, 1, 0);

        rtu_funCode = 23; // 强制将 03 号回复为 23 号功能码

        HToAChar(pwBuf + 3, &rtu_funCode, 1, 0);
        HToAChar(pwBuf + 5, (uint8_t *)&bytesCnt, 1, 0);

        // ulog_hexdump("rtu_test",16,ptrFrame,frame_len);
        // ulog_hexdump("dump_g_ctx",16,g_ctx->read_buf,100);
        // ulog_hexdump("dump_hold0",16,(uint8_t*)g_stConfig.scanTask[0].hold,100);
        log_d("asc_slaveAddr:%d , bytesCnt:%d", asc_slaveAddr, bytesCnt);

        // fixed：+2*ReadDatStAdd
        // HToAChar(pwBuf + 7, g_ctx->read_buf + 3 + 2 * ReadDatStAdd, bytesCnt);

        uint8_t temp_read_addr = ReadDatStAdd + g_stConfig.ascSys[asc_slaveAddr - 1].offset;
        // 地址有偏移

        if (ReadDatStAdd >= 256)
        {
            temp_read_addr = temp_read_addr - 256;
        }

        HToAChar(pwBuf + 7, (uint8_t *)g_stConfig.rtuSys.hold + 2 * (temp_read_addr), bytesCnt, 1);

        // ulog_hexdump("dump_pwbuf",16,pwBuf,100);

        // log_d("功能码之后 LRC 之前的字符:%s",pwBuf);
        // LRC 不包括 : \r\n
        // 2 * (3 + bytesCnt) == bufLen-5

        calc_lrc = ASCII_LRC(pwBuf + 1, calr_lrc_len);
        log_d("calc_lrc:0x%02X", calc_lrc);

        // 倒数第4个起
        HToAChar(pwBuf + bufLen - 4, (uint8_t *)&calc_lrc, 1, 0);
        rt_memcpy(pwBuf + bufLen - 2, "\r\n", 2);
        *(pwBuf + bufLen) = '\0';

        break;
    default:
        log_w("Not support asc_funcode:%d", asc_funcode);
        break;
    }

    log_d("ready2send:%s", pwBuf);

    // log_i("邮箱中消息的数目:%d",asc_resp_mb.entry);
    // 统一回复上位机
    // while (asc_resp_mb.entry > 0)
    // {

    // 将恢复消息发送到队列
    struct SER_MSG ser_msg;
    ser_msg.data_ptr = pwBuf;
    ser_msg.data_size = bufLen;

    uint32_t result = rt_mq_send(&asc_send_mq, &ser_msg, sizeof(struct SER_MSG));
    if (result != RT_EOK)
    {
        log_e("rt_mq_send asc_send_mq ERR\n");

        // 消息存入失败，将邮件扔掉
        struct SER_PORT *resp_port;
        if (rt_mb_recv(&asc_resp_mb, (rt_ubase_t *)&resp_port, 1 * RT_TICK_PER_SECOND) == RT_EOK)
        {
            log_d("resp_port->dev_name:%s", resp_port->dev_name);
            // rt_device_write(resp_port->device, 0, pwBuf, bufLen);
        }
    }

    // }
    // 统一都回复 23 号指令
    // response_ascii_frame(slaveAddr,23,pwBuf,bufLen);
    // log_i("send use time:%d", clock() - start);
    // rt_uint32_t level = rt_hw_interrupt_disable();
    // rt_hw_interrupt_enable(level);

    ret = 0;

exit:

    if (pwBuf)
        rt_free(pwBuf);

    if (pAscWbuf)
        rt_free(pAscWbuf);

    return ret;
}

struct RTU_FRAME
{
    uint8_t slaveAddr;
    uint8_t funCode;
    uint8_t byteCnt;
    uint8_t resovled;
    // // 本来想在这里定义为 uint16_t 的指针，后来发现反而不合适，需要把16 位数组装起来，然后还要拆开来，否则在转换为 ASCII 码的时候字节顺序不对
    // // 比如 0x1234 会变为 33 34 31 32 也就是 3412
    // uint8_t *pBytes;
    uint16_t crc;
};

// uint8_t parse_rtu_frame(char *ptrFrame, rt_uint16_t frame_len)
// {
//     rt_uint16_t i = 0;
//     char *pwBuf;

//     uint16_t rtu_recv_crc = 0;
//     uint16_t rtu_calc_crc = 0;

//     // log_d("开始解析 RTU frame:%d", frame_len);

//     ulog_hexdump("rtu_test", 16, ptrFrame, frame_len);

//     // 倒数两个字节是 crc，先计算一下，并判断收到的数据是否正确
//     rtu_recv_crc = *(ptrFrame + frame_len - 2) << 8 | *(ptrFrame + frame_len - 1);
//     rtu_calc_crc = CRC(ptrFrame, frame_len - 2);

//     log_d("rtu_recv_crc:%04X", rtu_recv_crc);
//     log_d("rtu_calc_crc:%04X", rtu_calc_crc);

//     if (rtu_recv_crc != rtu_calc_crc)
//     {
//         log_e("rtu_recv_crc ne rtu_calc_crc");
//         return -1;
//     }

//     uint8_t slaveAddr = *ptrFrame;
//     uint8_t funCode = *(ptrFrame + 1);

//     // log_d("slaveAddr:%d", slaveAddr);
//     log_d("parse_rtu_frame funCode:%d", funCode);

//     uint16_t bufLen = 0;
//     uint8_t bytesCnt = 0;        // 3 号功能码回复才有
//     uint16_t wrRegStartAddr = 0; // 16 号功能码回复才有 写入寄存器起始地址
//     uint16_t wrRegCnt = 0;       // 16 号功能码回复才有 写入寄存器数量
//     uint8_t rdRegCnt = 0;        // 23 号功能回复才有，读取结存器数量
//     uint8_t calc_lrc = 0;
//     //
//     switch (funCode)
//     {

//     case 3:
//         // 读回复
//         // 01 03 1A 00 0C 00 22 00 38 00 4E 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 00 42 C8
//         // : 01 17 1A 0328 0000 0000 0010 0000 0000 0000 0000 0000 0000 0000 0000 0000 93\CR\LF
//         // : 01 17 1A 000C 0022 0038 004E 000C 000C 000C 000C 000C 000C 000C 000C 0001 B9
//         bytesCnt = *(ptrFrame + 2);
//         // pwBuf 长度:是整个 frame 的长度，避免再次分配内存，这里一次分配
//         // 1(:)+(slaveAddr + funCode + bytesCnt + bytes)*2 + lrc + \r\n
//         bufLen = 1 + (1 + 1 + 1 + bytesCnt + 1) * 2 + 2;
//         // log_w("bufLen:%d",bufLen);
//         // 多分配 1 个字节，用于保存 '\0'
//         pwBuf = rt_malloc(bufLen + 1);
//         rt_memset(pwBuf, '\0', bufLen + 1);
//         // 写入 bytesCnt
//         *pwBuf = ':';
//         HToAChar(pwBuf + 1, &slaveAddr, 1, 0);

//         funCode = 23; // 强制将 03 号回复为 23 号功能码

//         HToAChar(pwBuf + 3, &funCode, 1, 0);
//         HToAChar(pwBuf + 5, &bytesCnt, 1, 0);
//         HToAChar(pwBuf + 7, ptrFrame + 3, bytesCnt, 1);
//         // log_d("功能码之后 LRC 之前的字符:%s",pwBuf);
//         // LRC 不包括 : \r\n
//         // 2 * (3 + bytesCnt) == bufLen-5
//         break;
//     case 16:
//         // 写回复
//         // 01 10 00 0C 00 01 C1 CA
//         // 如果要回复 16 号指令，那么需要另行考虑
//         wrRegStartAddr = *(ptrFrame + 2) << 8 | *(ptrFrame + 3);
//         wrRegCnt = *(ptrFrame + 4) << 8 | *(ptrFrame + 5);
//         // 如果 23 号指令只写不读，那么固定回复如下
//         // : 01 17 00 E8 \CR\LF
//         // pwBuf 长度:是整个 frame 的长度，避免再次分配内存，这里一次分配
//         // 1(:)+(slaveAddr + funCode + bytesReadCnts+lrc)*2
//         bufLen = 1 + (1 + 1 + 1 + 1) * 2 + 2;
//         log_w("bufLen:%d", bufLen);
//         pwBuf = rt_malloc(bufLen + 1);
//         rt_memset(pwBuf, '\0', bufLen + 1);
//         *pwBuf = ':';
//         HToAChar(pwBuf + 1, &slaveAddr, 1, 0);
//         funCode = 23; // 强制将 03 号回复为 23 号功能码
//         HToAChar(pwBuf + 3, &funCode, 1, 0);
//         HToAChar(pwBuf + 5, &rdRegCnt, 1, 0);
//         break;
//     default:
//         log_e("Not support");
//         break;
//     }

//     // 计算 lrc ，等于 bufLen - :(1) - lrc(2) - \r\n(2)
//     calc_lrc = ASCII_LRC(pwBuf + 1, bufLen - 5);
//     log_d("calc_lrc:0x%02X", calc_lrc);

//     // 倒数第4个起
//     HToAChar(pwBuf + bufLen - 4, (uint8_t *)&calc_lrc, 1, 0);
//     rt_memcpy(pwBuf + bufLen - 2, "\r\n", 2);
//     *(pwBuf + bufLen) = '\0';
//     // log_i("read2send:%s",pwBuf);

//     // struct SER_PORT resp_port;

//     //  RT_TICK_PER_SECOND/100

//     // 统一都回复 23 号指令
//     // response_ascii_frame(slaveAddr,23,pwBuf,bufLen);
//     if (pwBuf)
//         free(pwBuf);
// }

static rt_err_t uart_tx_com(rt_device_t dev, void *buffer)
{
    log_d("%s 发送完毕", dev->parent.name);
}

/* 接收数据回调函数 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    struct rx_msg msg;

    msg.dev = dev;
    msg.size = size;

    // rt_kprintf("msg.dev:%s\n", msg.dev);
    // rt_kprintf("msg.size:%d\n", msg.size);

    struct SER_PORT *port = NULL;

    /* 串口接收到数据后产生中断，调用此回调函数，然后发送接收信号量 */
    if (size > 0)
    {
        for (int i = 0; i < SER_PORTS_CNT; i++)
        {

            port = &g_stConfig.serPort[i];

            if (!rt_strcmp((char *)port->dev_name, msg.dev->parent.name))
            {

                rt_sem_release(&(port->rx_sem));

                // 与 linux 不同，这里不能直接调用 clock(),否则导致系统直接重启
                // 这里要通过信号量通知唤起线程去读取
            }
        }
    }
    return 0;
}

extern void serial_thread_entry(void *parameter);
#define THREAD_TIMESLICE 5
#define THREAD_PRIORITY 5
#define THREAD_STACK_SIZE 1024

int init_ser_ports()
{
    rt_err_t ret = RT_EOK;
    char uart_name[RT_NAME_MAX];
    struct serial_configure temp_config = RT_SERIAL_CONFIG_DEFAULT;
    cJSON *ports, *port, *scan, *tempObj;
    int iArrayCnt = 0;
    ports = cJSON_GetObjectItem(g_root, "serPorts");
    iArrayCnt = cJSON_GetArraySize(ports);
    log_d("iArrayCnt:%d", iArrayCnt);

    scan = cJSON_GetObjectItem(g_root, "scan");

    // bug: 这里获取不到 bool 的正真值，永远返回的都是 0！！！！！
    tempObj = cJSON_GetObjectItem(scan, "test");
    log_d("%s %d", tempObj->string, tempObj->valueint);
    tempObj = cJSON_GetObjectItem(scan, "test1");
    log_d("%s %d", tempObj->string, tempObj->valueint);

    // log_d("g_stConfig.scanEnable:%d", g_stConfig.scanEnable);
    // tempObj = cJSON_GetObjectItem(scan, "scanEnable");
    // if (NULL == tempObj)
    // {
    //     log_e("get item faild!");
    // }
    // g_stConfig.scanEnable = tempObj->valueint;
    // log_d("g_stConfig.scanEnable:%s %d,%d", tempObj->string, g_stConfig.scanEnable, tempObj->valueint);

    // tempObj = cJSON_GetObjectItem(scan, "scanInv");
    // g_stConfig.scanInv = tempObj->valueint;
    // log_d("g_stConfig.scanInv:%d", g_stConfig.scanInv);

    // tempObj = cJSON_GetObjectItem(scan, "scanStAddr");
    // g_stConfig.scanStAddr = tempObj->valueint;

    // tempObj = cJSON_GetObjectItem(scan, "scanRegCnt");
    // g_stConfig.scanRegCnt = tempObj->valueint;
    for (int i = 0; i < iArrayCnt; i++)
    {

        // 先赋值为默认值
        g_stConfig.serPort[i].config = temp_config;

        port = cJSON_GetArrayItem(ports, i);
        tempObj = cJSON_GetObjectItem(port, "name");
        log_d("default:g_stConfig.serPort[%d].dev_name %s", i, g_stConfig.serPort[i].dev_name);
        log_d("---get name:%s type:%d value:%s", tempObj->string, tempObj->type, tempObj->valuestring);

        rt_memset(g_stConfig.serPort[i].dev_name, '\0', 6);
        rt_strncpy(g_stConfig.serPort[i].dev_name, tempObj->valuestring, rt_strlen(tempObj->valuestring));
        log_d("new:g_stConfig.serPort[%d].dev_name %s", i, g_stConfig.serPort[i].dev_name);

        // 设置波特率
        tempObj = cJSON_GetObjectItem(port, "baudrate");
        log_d("default:g_stConfig.serPort[%d].config.baud_rate %d", i, g_stConfig.serPort[i].config.baud_rate);
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.baud_rate = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.baud_rate %d", i, g_stConfig.serPort[i].config.baud_rate);

        // 依据波特率，设置帧间隔
        // g_stConfig.serPort[i].frameInterval = 10000 / g_stConfig.serPort[i].config.baud_rate + 2 + 2;
        g_stConfig.serPort[i].frameInterval = 10;
        log_d("new:g_stConfig.serPort[%d].frameInterval %d", i, g_stConfig.serPort[i].frameInterval);

        // 设置数据位
        tempObj = cJSON_GetObjectItem(port, "databits");
        log_d("default:g_stConfig.serPort[%d].config.data_bits %d", i, g_stConfig.serPort[i].config.data_bits);
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.data_bits = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.data_bits %d", i, g_stConfig.serPort[i].config.data_bits);

        // 设置停止位
        tempObj = cJSON_GetObjectItem(port, "stopbits");
        log_d("default:g_stConfig.serPort[%d].config.stop_bits %d", i, g_stConfig.serPort[i].config.stop_bits);
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.stop_bits = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.stop_bits %d", i, g_stConfig.serPort[i].config.stop_bits);

        // 设置奇偶校验
        tempObj = cJSON_GetObjectItem(port, "parity");
        log_d("default:g_stConfig.serPort[%d].config.parity %d", i, g_stConfig.serPort[i].config.parity);
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.parity = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.parity %d", i, g_stConfig.serPort[i].config.parity);

        // 设置 bufsz
        tempObj = cJSON_GetObjectItem(port, "bufsz");
        log_d("default:g_stConfig.serPort[%d].config.bufsz %d", i, g_stConfig.serPort[i].config.bufsz);
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.bufsz = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.bufsz %d", i, g_stConfig.serPort[i].config.bufsz);

        rt_kprintf("dev_name:%s\n", g_stConfig.serPort[i].dev_name);
        rt_strncpy(uart_name, g_stConfig.serPort[i].dev_name, RT_NAME_MAX);

        g_stConfig.serPort[i].device = rt_device_find(uart_name);

        if (g_stConfig.serPort[i].device == NULL)
        {
            rt_kprintf("Can't find %s.\n", uart_name);
            return RT_ERROR;
            // goto exit;
        }
        rt_kprintf("Find %s.\n", uart_name);

        // https://www.rt-thread.org/document/site/#/rt-thread-version/rt-thread-standard/programming-manual/device/uart/uart_v1/uart

        rt_device_control(g_stConfig.serPort[i].device, RT_DEVICE_CTRL_CONFIG, &g_stConfig.serPort[i].config);

        /* 发送字符串 */
        // char str[]="hello";
        // rt_device_write(serial, 0, str, (sizeof(str) - 1));

        /* 设置接收回调函数 */
        rt_device_set_rx_indicate(g_stConfig.serPort[i].device, uart_rx_ind);

        /* 设置发送完成回调函数 没有作用，驱动不支持*/
        rt_device_set_tx_complete(g_stConfig.serPort[i].device, uart_tx_com);

        /* 没有作用，驱动不支持*/
        // rt_device_control(g_stConfig.serPort[i].device, RT_DEVICE_CTRL_SET_INT,RT_DEVICE_FLAG_INT_TX);

        /* 初始化信号量 */
        char sem_name[10] = {'\0'};
        sprintf(sem_name, "rx_%s", g_stConfig.serPort[i].dev_name);
        rt_sem_init(&g_stConfig.serPort[i].rx_sem, sem_name, 0, RT_IPC_FLAG_FIFO);

        /* Interrupt RX */
        ret = rt_device_open(g_stConfig.serPort[i].device, RT_DEVICE_FLAG_INT_RX);
        RT_ASSERT(ret == RT_EOK);

        memset(g_stConfig.serPort[i].rx_buf, 0, MAX_BUF_LENGTH);
        g_stConfig.serPort[i].CanRecv = MAX_BUF_LENGTH;
        char thread_name[10] = {'\0'};

        sprintf(thread_name, "th_%s", g_stConfig.serPort[i].dev_name);

        // 对 RTU 口不在再通过线程被动读取，而是采用主动控制的方式
        if (i == 0)
            continue;
        /* 创建 serial 线程 */

        rt_thread_t thread = rt_thread_create(thread_name, (void (*)(void *parameter))serial_thread_entry, (void *)i, THREAD_STACK_SIZE, THREAD_PRIORITY, THREAD_TIMESLICE);
        /* 创建成功则启动线程 */
        if (thread != RT_NULL)
        {
            rt_thread_startup(thread);
        }
        else
        {
            ret = RT_ERROR;
            ret = rt_device_close(g_stConfig.serPort[i].device);
            goto exit;
        };
    }

    return 0;

exit:
    RT_ASSERT(ret == RT_EOK);
    return ret;
}

int rs485_send(rt_device_t dev, uint8_t *buf, int len)
{

    // struct SER_PORT *port = &g_stConfig.serPort[0];
    // rt_device_write(port->device, 0, buf, len);

    // ulog_hexdump("rs485_send", 16, buf, len);
    rt_device_write(dev, 0, buf, len);

    // if (!rt_strcmp((char *)dev->parent.name, "uart6"))
    // {
    //  log_e("%s 发送完毕",dev->parent.name);
    // }
    return len;
}
// timeout:要求下位机必须在 timeout 时间内回复，否则会认为读取超时
int rs485_receive(uint8_t *buf, int bufsz, int timeout)
{
    int len = 0;
    struct SER_PORT *port = &g_stConfig.serPort[0];

    while (1)
    {
        rt_sem_control(&g_stConfig.serPort[0].rx_sem, RT_IPC_CMD_RESET, RT_NULL);

        int rc = rt_device_read(port->device, 0, buf + len, bufsz);
        // log_d("rc:%d", rc);
        // log_d("bufsz:%d", bufsz);

        if (rc > 0)
        {
            // timeout = bytes_timeout;
            len += rc;
            bufsz -= rc;
            if (bufsz == 0)
                break;
            continue;
        }

        if (rt_sem_take(&g_stConfig.serPort[0].rx_sem, rt_tick_from_millisecond(timeout)) != RT_EOK)
        {
            // log_w("读取超时");
            break;
        }
        // timeout = bytes_timeout;
    }

    // ulog_hexdump("rs485_receive", 16, buf, len);

    return len;
}

void rtu_master_init(void)
{
    // 13*16 - 3 =  208
    //
    // agile_modbus_rtu_t ctx_rtu;
    // agile_modbus_t *ctx = &ctx_rtu._ctx;
    agile_modbus_rtu_init(&g_ctx_rtu, ctx_send_buf, sizeof(ctx_send_buf), ctx_read_buf, sizeof(ctx_read_buf));
    agile_modbus_set_slave(g_ctx, g_stConfig.rtuSys.rtuAddr);
}