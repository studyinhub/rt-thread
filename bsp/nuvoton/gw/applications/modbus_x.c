#include "modbus_x.h"
#include "config.h"

#include "mb.h"
#include "mb_m.h"
#include "user_mb_app.h"

#include "app_config.h"

#define MB_POLL_THREAD_PRIORITY 10
#define MB_SEND_THREAD_PRIORITY RT_THREAD_PRIORITY_MAX - 1

#define LOG_TAG              "modbusx"
#define LOG_LVL              LOG_LVL_DBG
#include <ulog.h>


int RAddLimitMax = 15; //允许读寻址的最大值
int RAddLimitMin = 0;  //允许读寻址的最小值
int WAddLimitMax = 16; //允许写寻址的最大值
int WAddLimitMin = 0;  //允许写寻址的最小

extern USHORT usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];

struct CONFIG g_stConfig = {
    .masterID = 1,
    .slaveAddr = 1,
    .startRegAddr = 0,
    .regsCnt = 10,
    .serPort = {
        {"uart1", 1, "ascii", RT_SERIAL_CONFIG_DEFAULT}, // ASCII-RS232 slave
        {"uart6", 1, "ascii", RT_SERIAL_CONFIG_DEFAULT}, // ASCII-RS485 slave
        {"uart8", 0, "rtu", RT_SERIAL_CONFIG_DEFAULT}}   // RTU-RS485 master
};

static void send_thread_entry(void *parameter)
{
    eMBMasterReqErrCode error_code = MB_MRE_NO_ERR;
    rt_uint16_t error_count = 0;
    USHORT data[2] = {0};
    LOG_I("send_thread_entry");

    while (1)
    {
        /* Test Modbus Master */
        data[0] = (USHORT)(rt_tick_get() / 10);
        data[1] = (USHORT)(rt_tick_get() % 10);

        // error_code = eMBMasterReqWriteMultipleHoldingRegister(g_stConfig.slaveAddr,    /* salve address */
        //                                                       g_stConfig.startRegAddr, /* register start address */
        //                                                       g_stConfig.regsCnt,      /* register total number */
        //                                                       data,                /* data to be written */
        //                                                       RT_WAITING_FOREVER); /* timeout */

        eMBMasterReqReadHoldingRegister(g_stConfig.slaveAddr,
                                        g_stConfig.startRegAddr,
                                        g_stConfig.regsCnt,
                                        500);

        // for(int i=0;i<g_stConfig.regsCnt;i++)
        // {
        //     rt_kprintf("usMRegHoldBuf[%d]:%d\n",i,usMRegHoldBuf[0][i]);
        // }

        /* Record the number of errors */
        if (error_code != MB_MRE_NO_ERR)
        {
            error_count++;
        }
    }
}

char ATOHChar(char *var) //ASCII 字符串 转8位16进制数 '12'转为0x12
{
    char var1 = *var;
    char var2 = *(var + 1);
    char temp = 0;
    if (var1 >= 0x30 && var1 <= 0x39)
        temp = var1 - '0';
    else if (var1 >= 0x30 && var1 <= 0x39)
        temp = var1 - 'A' + 10;

    temp = temp << 4;

    if (var2 >= 0x30 && var2 <= 0x39)
        temp = temp + var2 - '0';
    else if (var2 >= 'A' && var2 <= 'F')
        temp = temp + var2 - 'A' + 10;
    return temp;
}

// LRC checks the content of the message other than [:] of START and [CR][LF] of END. The sending side calculates and sets. The receiving side calculates based on the received message, and compares the calculation result with the received LRC. The received message is deleted if the calculation result and received LRC do not match.
// Add up the byte number of the message consisting of 8 consecutive bits. The result except the carry (overflow) is converted to 2’s complement.
// http://www.ip33.com/lrc.html
// 如果内容本身是已经 ASCII 转换过后的 buf
int HEX_LRC(uint8_t *buf, uint8_t len)
{
    int result = 0;
    LOG_D("HEX_LRC:");
    for (int i = 0; i < len; i++)
    {
        rt_kprintf("%02x ", *(buf + i));
        result += *(buf + i);
    }
    rt_kprintf("\n");

    return 256 - (result % 256);
}

// 如果是对 ASCII buf 直接进行计算，则需要先收聚 ASCII，然后再计算
int ASCII_LRC(uint8_t *buf, uint8_t len)
{
    int result = 0;
    // 先要对 ASCII 转换为 16 进制
    // 必须是 2 的倍数

    LOG_D("ASCII_LRC(%d):", len);

    for (int j = 0; j < len; j++)
    {
        rt_kprintf("%02x ", *(buf + j));
    }

    rt_kprintf("\n");

    if (len % 2 != 0)
    {
        LOG_W("len is not mod by 2,please check");
        return -1;
    }

    uint8_t *hBuf;
    uint8_t hLen = len / 2;

    LOG_D("hLen:%d", hLen);

    hBuf = rt_malloc(hLen);
    rt_memset(hBuf, 0, hLen);

    for (int i = 0; i < hLen; i++)
    {
        *(hBuf + i) = ATOHChar(buf + i * 2);

        rt_kprintf("%02x ", *(hBuf + i));
    }
    rt_kprintf("\n");

    result = HEX_LRC(hBuf, len / 2);

    rt_free(hBuf);

    return result;
}

int ATOHInt(char *var) //ASCII 字符串 转16位16进制数 '12AB'转为0x12AB
{
    char var1 = *var;
    char var2 = *(var + 1);
    char var3 = *(var + 2);
    char var4 = *(var + 3);

    int temp = 0;
    if (var1 >= 0x30 && var1 <= 0x39)
        temp = var1 - '0';
    else if (var1 >= 0x30 && var1 <= 0x39)
        temp = var1 - 'A' + 10;
    temp = temp << 4;
    if (var2 >= 0x30 && var2 <= 0x39)
        temp = temp + var2 - '0';
    else if (var2 >= 'A' && var2 <= 'F')
        temp = temp + var2 - 'A' + 10;
    temp = temp << 4;
    if (var3 >= 0x30 && var3 <= 0x39)
        temp = temp + var3 - '0';
    else if (var3 >= 'A' && var3 <= 'F')
        temp = temp + var3 - 'A' + 10;
    temp = temp << 4;
    if (var4 >= 0x30 && var4 <= 0x39)
        temp = temp + var4 - '0';
    else if (var4 >= 'A' && var4 <= 'F')
        temp = temp + var4 - 'A' + 10;

    return temp;
}

int HToAChar(char *pDstAsc, uint8_t *pSrcHex, uint8_t len) //4位16进制数，转ASCII字符  0,1,2,3 转 ‘0’，‘1’，‘2’，‘3’
{
    char temp = 0;
    char nibble[2]; // nibble 半字节的意思
    int i = 0, j = 0;

    char buffer[128];

    for (i = 0; i < len; i++)
    {
        nibble[0] = pSrcHex[i] >> 4 & 0x0F;
        nibble[1] = pSrcHex[i] & 0x0F;
        for (j = 0; j < 2; j++)
        {
            if (nibble[j] < 10)
            {
                nibble[j] += 0x30;
            }
            else if (nibble[j] < 16)
            {
                nibble[j] = nibble[j] - 10 + 'A';
            }
            else
            {
                return 0;
            }
        }
        memcpy(buffer + i * 2, nibble, 2);
    }

    buffer[2 * len] = 0x00;
    memcpy(pDstAsc, buffer, 2 * len);
    pDstAsc[2 * len] = 0x00;
    return 1;
}


/* 接收数据回调函数 */
static rt_err_t uart_rx_ind(rt_device_t dev, rt_size_t size)
{
    struct rx_msg msg;
    rt_err_t result = RT_EOK;

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
    return result;
}



static void mbpoll_thread_entry(void *parameter)
{
    eMBMasterInit(MB_RTU, MB_MASTER_USING_PORT_NUM, MB_MASTER_USING_PORT_BAUDRATE, MB_PAR_NONE);
    eMBMasterEnable();
    while (1)
    {
        eMBMasterPoll();
        rt_thread_mdelay(MB_POLL_CYCLE_MS);
    }
}

static void serial_thread_entry(void *parameter)
{
    char ch;
    int index = (int)parameter, i = 0, j = 0;
    LOG_D("%s read thread started\n", g_stConfig.serPort[index].dev_name);

    int ReadDatStAdd;  //上位机要的读的数据起始地
    int ReadDatLenth;  //上位机要读的数据的长度
    int WriteDatStAdd; //上位机要写的数据的起始地址
    int WriteDatLenth; //上位机要写的数据的长度
    int WriteByteNum;  //上位机要写的数据的字节长度=数据长度*2
    uint8_t slaveAddr; //上位机请求中的从机地址
    uint8_t funcode;   //上位机请求中的功能码
    uint8_t lrc;       //上位机请求中的 LRC

    static int f_index = 0;
    clock_t start = 0, end = 0;
    int readlen = 0;
    int buflen = 0;
    uint8_t calc_lrc = 0;

    struct SER_PORT *port = &g_stConfig.serPort[index];
    char *prBuf = port->rx_buf;
    char *pwBuf = port->tx_buf;

    while (1)
    {
        // 等待数据接收完成
        rt_sem_control(&g_stConfig.serPort[index].rx_sem, RT_IPC_CMD_RESET, RT_NULL);
        /* 阻塞等待接收信号量，等到中断后再次读取数据 */
        rt_sem_take(&g_stConfig.serPort[index].rx_sem, RT_WAITING_FOREVER);

        if (!rt_strcmp(port->prot, "ascii"))
        {

            start = clock();
            // 从第1个字节开始计时，超过 10ms，就终止本次读取
            while (1)
            {

                // : 01 17 0004 0003 000B 0002 04 009B 0001 34

                if (port->CanRecv <= 1)
                    port->CanRecv = 1;

                // rt_kprintf("CanRecv:%d\n", port->CanRecv);
                readlen = rt_device_read(port->device, 0, prBuf + MAX_BUF_LENGTH - port->CanRecv, port->CanRecv);
                // rt_kprintf("readlen:%d\n", readlen);
                if (readlen > 0)
                {
                    start = clock(); //收到数据，重新开始计时，返回值单位：毫秒
                    port->CanRecv = port->CanRecv - readlen;
                }
                else
                {
                    end = clock();
                    if ((end - start) > 5) //间隔超时 5ms
                        break;
                }
            }

            buflen = MAX_BUF_LENGTH - port->CanRecv; //接收到的总字节数

            rt_kprintf("buf:%s\n", prBuf);

            // 处理 buf 中的所有数据

            for (i = 0; i < buflen; i++)
            {
                // 查找帧头
                if (*(prBuf + i) != ':')
                {
                    continue;
                }
                j = 1;
                slaveAddr = ATOHChar(prBuf + i + j);
                j += 2;
                if (slaveAddr != port->slaveAddr)
                {
                    LOG_W("Please check the slave addr");
                    continue;
                }

                LOG_D("slaveAddr:%d", slaveAddr);

                // 查找帧尾,不查找
                // 判断是否是 0x17

                // printf("%d(%02x),%d(%02x)\n",*(prBuf+i+3),*(prBuf+i+3),*(prBuf+i+4),*(prBuf+i+4));

                funcode = ATOHChar(prBuf + i + j);
                j += 2;

                LOG_D("funcode:0x%02x", funcode);
                if (funcode != 0x17) //功能代码符合0x17
                {
                    LOG_W("Only support 0x17 but got 0x%02x", funcode);
                    continue;
                }

                ReadDatStAdd = ATOHInt(prBuf + i + j);
                j += 4;

                LOG_D("ReadDatStAdd:%04x", ReadDatStAdd);
                //功能代码23（0x17)要读取的数据起始地址，占4个字节[3]~[6]
                ReadDatLenth = ATOHInt(prBuf + i + j);
                j += 4;
                LOG_D("ReadDatLenth:%04x", ReadDatLenth);

                //功能代码23（0x17)要读取的数据长度，占4个字节[7]~[10]
                if (!(ReadDatStAdd >= RAddLimitMin && ReadDatStAdd <= RAddLimitMax && ReadDatStAdd + ReadDatLenth >= RAddLimitMin + 1 && ReadDatStAdd + ReadDatLenth <= RAddLimitMax + 1)) //读的地址和长度符合
                    //起始地址不能超范围，起始地址+数据长度不能超范围
                {
                    LOG_W("Invalid read parameters");
                    continue;
                }

                WriteDatStAdd = ATOHInt(prBuf + i + j);
                j += 4;
                LOG_D("WriteDatStAdd:%04x", WriteDatStAdd); //功能代码23（0x17)要写取的数据起始地址，占4个字节[11]~[14]

                WriteDatLenth = ATOHInt(prBuf + i + j); //功能代码23（0x17)要写取的寄存器数量，占4个字节[15]~[18]
                j += 4;
                LOG_D("WriteDatLenth:%04x", WriteDatLenth);

                WriteByteNum = ATOHChar(prBuf + i + j); //功能代码23（0x17)要写取的数据字节个数，占2个字节[19]~[20]
                j += 2;
                LOG_D("WriteByteNum:%02x", WriteByteNum);

                if (!(WriteByteNum == WriteDatLenth * 2 && (WriteDatLenth == 0 || (WriteDatLenth > 0 && WriteDatStAdd >= WAddLimitMin && WriteDatStAdd <= WAddLimitMax && WriteDatStAdd + WriteDatLenth >= WAddLimitMin + 1 && WriteDatStAdd + WriteDatLenth <= WAddLimitMax + 1)))) //写的地址和长度符合
                {
                    //写的数据的起始地址不能超范围，写的数据起始地址+数据长度不能超范围
                    LOG_W("Invalid read parameters");
                    continue;
                }

                j += WriteByteNum * 2; //  注意这里是 * 2 ，不是 *4，长度 4 个字节，ASCII 的表示是翻倍的，占用 4*2 个 字节。
                LOG_D("j:%d", j);

                uint8_t test[5] = {0x01, 0xA0, 0x7C, 0xFF, 0x02};

                calc_lrc = ASCII_LRC(prBuf + 1, j - 1); // j-1 表示不计算 ：
                LOG_D("calc_lrc:%02x", calc_lrc);

                lrc = ATOHChar(prBuf + i + j);
                j += 2;
                LOG_D("lrc:%02x", lrc);

                if (calc_lrc != lrc || calc_lrc == -1) //LRC校验和判断 -1 表示计算失败
                {
                    LOG_W("LRC mismatch!");
                    continue;
                }

                if (!(*(prBuf + i + j) == 0x0D && *(prBuf + i + j + 1) == 0x0A)) //结束符0x0D,和0x0A符合
                {
                    LOG_W("Not found 0x0D && 0x0A");
                    continue;
                }
                LOG_D("Valid!");
                // :011706000000000000E2
                // reset  j
                j = 0;
                memcpy(pwBuf, prBuf, 5); // 复制接收到的: slaveAddr 和 funCode 共 5 个字节
                j += 5;
                //
                LOG_D("read from %04x for %04x quantity register", ReadDatStAdd, ReadDatLenth);
                // 读取 3 个寄存器，每个寄存器共 2 个字节，共 6 个字节，12 个 Ascii 字符
                uint8_t temp_read_cnt = ReadDatLenth * 2;
                HToAChar(pwBuf + j, &temp_read_cnt, 1);
                j += 2;

                uint16_t *pRespdata;

                pRespdata = rt_malloc(ReadDatLenth * sizeof(uint16_t));
                // 这里设置测试数据
                *pRespdata = 0x0000;
                *(pRespdata + 1) = 0x000;
                *(pRespdata + 2) = 0x0000;

                // 转换为 ascii

                HToAChar(pwBuf + j, (uint8_t *)pRespdata, 6);
                j += 12;
                // 补充上 LRC
                calc_lrc = ASCII_LRC(pwBuf + 1, j - 1); // j-1 表示不计算 ：

                HToAChar(pwBuf + j, (uint8_t *)&calc_lrc, 1);
                j += 2;

                rt_memcpy(pwBuf + j, "\r\n", 2);
                j += 2;

                LOG_D("resp:%d", j);
                // =j 表示看最后一个字节是否为 0
                for (int k = 0; k <= j; k++)
                {
                    rt_kprintf("%02x ", *(pwBuf + k));
                }

                rt_device_write(port->device, 0, pwBuf, j);

                free(pRespdata);

                if (buflen - i < 21 + WriteByteNum * 4 + 4) // 如果轮循剩下的字节不足21+WriteByteNum则停止轮循，退出，接收数据不完整
                    break;

                //如果都符合,则准备给上位机回复上位机想要的数据
                // //modbus Ascii
                // TXBuff_Rs458A[0] = ':';                                                                                                  //起始位“:”
                // TXBuff_Rs458A[1] = HToAChar(MasterID / 0x10);                                                                            //设备地址高4位
                // TXBuff_Rs458A[2] = HToAChar(MasterID % 0x10);                                                                            //设备地址低4位
                // TXBuff_Rs458A[3] = '1';                                                                                                  //功能代码地址高4位
                // TXBuff_Rs458A[4] = '7';                                                                                                  //功能代码地址低4位
                // TXBuff_Rs458A[5] = HToAChar(char(ReadDatLenth * 2) / 0x10);                                                              //数据字节长度高4位
                // TXBuff_Rs458A[6] = HToAChar(char(ReadDatLenth * 2) % 0x10);                                                              //数据字节长度低4位
                // sum = TXBuff_Rs458A[1] + TXBuff_Rs458A[2] + TXBuff_Rs458A[3] + TXBuff_Rs458A[4] + TXBuff_Rs458A[5] + TXBuff_Rs458A[6] ； //前面6个数的LRC校验和
                //       for (k = 0; k < ReadDatLenth; k++)                                                                                 //每个数据占四个字节
                // {
                //     TXBuff_Rs458A[7 + k * 4] = HToAChar(DataReg[ReadDatStAdd] / 0x1000 % 0x10);
                //     sum = sum + TXBuff_Rs458A[7 + k * 4];
                //     TXBuff_Rs458A[8 + k * 4] = HToAChar(DataReg[ReadDatStAdd] / 0x100 % 0x10);
                //     sum = sum + TXBuff_Rs458A[8 + k * 4];
                //     TXBuff_Rs458A[9 + k * 4] = HToAChar(DataReg[ReadDatStAdd] / 0x10 % 0x10);
                //     sum = sum + TXBuff_Rs458A[9 + k * 4];
                //     TXBuff_Rs458A[10 + k * 4] = HToAChar(DataReg[ReadDatStAdd] % 0x10);
                //     sum = sum + TXBuff_Rs458A[10 + k * 4];
                // }
                // TXBuff_Rs458A[7 + k * 4] = HToAChar(sum / 0x10); //LRC校验和高4位,注意此时的k等于ReadDatLenth
                // TXBuff_Rs458A[8 + k * 4] = HToAChar(sum % 0x10); //LRC校验和低4位
                // TXBuff_Rs458A[9 + k * 4] = 0x0d;                 //回车
                // TXBuff_Rs458A[10 + k * 4] = 0x0a;                //换行
                // sendLenth = 11 + k * 4;                          //发送字节长度
                // SentA(TXBuff_Rs458A, sendLenth);
                // memset(TXBuff_Rs458A, '\0', sizeof(TXBuff_Rs458A));         //清空RS485A接收缓存
                // if (WriteByteNum == WriteDatLenth * 2 && WriteDatLenth > 0) //如果上位机有写数据，则将接收到的要写的数据先放到WDataReg[]数组中，然后给控制器发送16指令，将这些数据写给控制器,如果没有则不执行
                // {                                                           //RTU
                //     TXBuff_Rs458B[0] = SlaverID;                            //控制器通讯地址
                //     TXBuff_Rs458B[1] = 0x10;                                //这里需要注意，控制器支不支持0x10指令，如果不支持，则需要通过0x06将多个数据多次发送
                //     TXBuff_Rs458B[2] = WriteDatStAdd / 0x100;               //给控制器写的起始地址高8位
                //     TXBuff_Rs458B[3] = WriteDatStAdd % 0x100;               //给控制器写的起始地址低8位
                //     TXBuff_Rs458B[4] = WriteDatLenth % 0x100;               //给控制器写的数据个数高8位
                //     TXBuff_Rs458B[5] = WriteDatLenth % 0x100;               //给控制器写的数据个数低8位
                //     TXBuff_Rs458B[6] = WriteByteNum;                        //给控制器写的数据的总字节数
                //     for (k = 0; k < WriteDatLenth; k++)                     //每个写的数据占两个字节
                //     {
                //         WDataReg[WriteDatStAdd + k] = ATOHInt(port->rx_buf[i + 21 + k * 4], port->rx_buf[i + 22 + k * 4], port->rx_buf[i + 23 + k * 4], port->rx_buf[i + 24 + k * 4]);
                //         TXBuff_Rs458B[7 + k * 2] = WDataReg[WriteDatStAdd + k] / 0x100;
                //         TXBuff_Rs458B[8 + k * 2] = WDataReg[WriteDatStAdd + k] % 0x100;
                //     }
                //     crc = CRC(TXBuff_Rs458B, 7 + k * 2);    //CRC校验，7+k*2是校验的长度
                //     TXBuff_Rs458B[7 + k * 2] = crc / 0x100; //校验值高8位
                //     TXBuff_Rs458B[8 + k * 2] = crc % 0x100; //校验值低8位
                //     sendLenth = 9 + k * 2                   //发送的总字节数

                //                         SentB(TXBuff_Rs458B, sendLenth);
                //     memset(TXBuff_Rs458B, '\0', sizeof(TXBuff_Rs458B)); //清空RS485B发送缓存
            }

            memset(port->rx_buf, 0, MAX_BUF_LENGTH);
            port->CanRecv = MAX_BUF_LENGTH;
        }
        else
        {
            // RTU 口收到数据
        }
    }
}

int init_ser_ports()
{
    rt_err_t ret = RT_EOK;
    char uart_name[RT_NAME_MAX];

    struct serial_configure temp_config = RT_SERIAL_CONFIG_DEFAULT;
    
    cJSON *ports;
    cJSON *port, *tempObj;

    ports = cJSON_GetObjectItem(g_root, "ports");
    int iArrayCnt = cJSON_GetArraySize(ports);
    log_d("iArrayCnt:%d", iArrayCnt);

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
        log_d("default:g_stConfig.serPort[%d].config.baud_rate %d", i, g_stConfig.serPort[i].config.baud_rate);
        tempObj = cJSON_GetObjectItem(port, "baud_rate");
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.baud_rate = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.baud_rate %d", i, g_stConfig.serPort[i].config.baud_rate);

        // 设置数据位
        log_d("default:g_stConfig.serPort[%d].config.data_bits %d", i, g_stConfig.serPort[i].config.data_bits);
        tempObj = cJSON_GetObjectItem(port, "data_bits");
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.data_bits = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.data_bits %d", i, g_stConfig.serPort[i].config.data_bits);

        // 设置停止位
        log_d("default:g_stConfig.serPort[%d].config.stop_bits %d", i, g_stConfig.serPort[i].config.stop_bits);
        tempObj = cJSON_GetObjectItem(port, "stop_bits");
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.stop_bits = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.stop_bits %d", i, g_stConfig.serPort[i].config.stop_bits);

        // 设置奇偶校验
        log_d("default:g_stConfig.serPort[%d].config.parity %d", i, g_stConfig.serPort[i].config.parity);
        tempObj = cJSON_GetObjectItem(port, "parity");
        log_d("---get name:%s type:%d value:%d", tempObj->string, tempObj->type, tempObj->valueint);
        g_stConfig.serPort[i].config.parity = tempObj->valueint;
        log_d("new:g_stConfig.serPort[%d].config.parity %d", i, g_stConfig.serPort[i].config.parity);

        // 设置 bufsz
        log_d("default:g_stConfig.serPort[%d].config.bufsz %d", i, g_stConfig.serPort[i].config.bufsz);
        tempObj = cJSON_GetObjectItem(port, "bufsz");
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
            //goto exit;
        }
        rt_kprintf("Find %s.\n", uart_name);

        // g_stConfig.serPort[i].config.baud_rate = BAUD_RATE_9600; //修改波特率为 115200
        // g_stConfig.serPort[i].config.data_bits = DATA_BITS_8;    //数据位 8
        // g_stConfig.serPort[i].config.stop_bits = STOP_BITS_1;    //停止位 1
        // g_stConfig.serPort[i].config.bufsz = 128;                //修改缓冲区 buff size 为 128
        // g_stConfig.serPort[i].config.parity = PARITY_NONE;       //无奇偶校验位

        rt_device_control(g_stConfig.serPort[i].device, RT_DEVICE_CTRL_CONFIG, &g_stConfig.serPort[i].config);

        /* 发送字符串 */
        // char str[]="hello";
        // rt_device_write(serial, 0, str, (sizeof(str) - 1));

        char thread_name[10] = {'\0'};
        // 如果是 ascii 端口则需要通中断方式来获取数据
        if (!rt_strcmp(g_stConfig.serPort[i].prot, "ascii"))
        {
            /* 设置接收回调函数 */
            rt_device_set_rx_indicate(g_stConfig.serPort[i].device, uart_rx_ind);

            /* 初始化信号量 */
            char sem_name[10] = {'\0'};
            sprintf(sem_name, "rx_%s", g_stConfig.serPort[i].dev_name);
            rt_sem_init(&g_stConfig.serPort[i].rx_sem, sem_name, 0, RT_IPC_FLAG_FIFO);

            /* Interrupt RX */
            ret = rt_device_open(g_stConfig.serPort[i].device, RT_DEVICE_FLAG_INT_RX);
            RT_ASSERT(ret == RT_EOK);

            memset(g_stConfig.serPort[i].rx_buf, 0, MAX_BUF_LENGTH);

            g_stConfig.serPort[i].CanRecv = MAX_BUF_LENGTH;

            sprintf(thread_name, "th_%s", g_stConfig.serPort[i].dev_name);
            /* 创建 serial 线程 */
            rt_thread_t thread = rt_thread_create(thread_name, (void (*)(void *parameter))serial_thread_entry, (void *)i, 1024, 25, 10);
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
            }
        }
        else
        {
            sprintf(thread_name, "mbp_%s", g_stConfig.serPort[i].dev_name);
            LOG_D("This port (%s) is rtu,will as rtu master", g_stConfig.serPort[i].dev_name);
            rt_thread_t thread_mbpoll = rt_thread_create(thread_name, (void (*)(void *parameter))mbpoll_thread_entry, (void *)i, 1024, MB_POLL_THREAD_PRIORITY, 10);
            if (thread_mbpoll != RT_NULL)
            {
                rt_thread_startup(thread_mbpoll);
            }

            // 轮询数据线程
            rt_thread_t thread_mbsend = rt_thread_create("md_m_send", send_thread_entry, RT_NULL, 512, MB_SEND_THREAD_PRIORITY, 10);
            if (thread_mbsend != RT_NULL)
            {
                rt_thread_startup(thread_mbsend);
            }
        }
    }

    return 0;

exit:
    RT_ASSERT(ret == RT_EOK);
    return ret;
}