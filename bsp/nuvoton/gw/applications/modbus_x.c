#include "modbus_x.h"
#include "config.h"
#include "mb.h"
#include "mb_m.h"
#include "user_mb_app.h"
#include "app_config.h"

#include "crc16.h"
#include "utils.h"


#define LOG_TAG "modbusx"
#define LOG_LVL LOG_LVL_DBG
#include <ulog.h>




// ASCII 接收消息队列
static struct rt_messagequeue asc_recv_mq;
static rt_uint8_t asc_msg_pool[2048];
// RTU 接收响应消息队列
static struct rt_messagequeue rtu_recv_mq;
static rt_uint8_t rtu_msg_pool[2048];

ALIGN(RT_ALIGN_SIZE)
static char thread_asc_stack[1024];
static struct rt_thread thread_asc;

ALIGN(RT_ALIGN_SIZE)
static char thread_rtu_stack[1024];
static struct rt_thread thread_rtu;

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

// 01 03 00 01 00 01 D5 CA
// 01 03 00 00 00 0A C5 CD

void send_rtu_frame(uint8_t slaveaddr, uint8_t funcode, uint16_t readStart, uint16_t readCnt)
{
    char *pwBuf;
    int i = 0;
    uint16_t crc;

    log_i("发送功能码%d到%d", funcode, slaveaddr);

    pwBuf = rt_malloc(24);
    struct SER_PORT *port = &g_stConfig.serPort[2];

    *pwBuf = slaveaddr;
    i += 1;

    *(pwBuf + i) = funcode;
    i += 1;

    *(pwBuf + i) = (readStart >> 8) && 0xFF;
    i += 1;

    *(pwBuf + i) = readStart & 0x00ff;
    i += 1;

    *(pwBuf + i) = (readCnt >> 8) && 0xFF;
    i += 1;

    *(pwBuf + i) = readCnt & 0x00FF;
    i += 1;

    crc = CRC(pwBuf, i);        //CRC校验，7+k*2是校验的长度
    *(pwBuf + i) = crc / 0x100; //校验值高8位
    i += 1;

    *(pwBuf + i) = crc % 0x100; //校验值低8位
    i += 1;

    for (int j = 0; j < i; j++)
    {
        rt_kprintf("%02X ", *(pwBuf + j));
    }
    rt_kprintf("\n");

    rt_device_write(port->device, 0, pwBuf, i);
    free(pwBuf);
}

// :011706000000000000E2

// :01 17 1A 0328 0000 0000 0010 0000 0000 0000 0000 0000 0000 0000 0000 0000 93\CR\LF
void response_ascii_frame(uint8_t slaveAddr, uint8_t *buf, uint16_t byteCnt)
{
    char *pwBuf;
    uint8_t calc_lrc = 0;
    int i = 0;
    log_i("response_ascii_frame:%d", byteCnt);

    // i = 1;
    // unsigned char *pointer;
    // pointer = (unsigned char *)&i;

    // if (*pointer)
    // {
    //     log_w("litttle_endian");
    // }
    // else
    // {
    //     log_w("big endian/n");
    // }

    // for (i = 0; i < byteCnt; i++)
    // {
    //     log_d("i:%d v:%02X:", i, *(buf + i));
    // }

    // :+（slaveAddr + funcode + quantity bytes + lrc） * 2 + datalen + '\r\n'
    // 其中 datalen 已经是字节数了
    uint16_t frame_len = 1 + (1 + 1 + 1 + 1 + byteCnt) * 2 + 2;
    log_d("frame_len:%d", frame_len);

    pwBuf = rt_malloc(frame_len);
    rt_memset(pwBuf, '\0', frame_len);

    uint8_t funCode = 0x17;

    *pwBuf = ':';
    HToAChar(pwBuf + 1, &slaveAddr, 1);
    HToAChar(pwBuf + 3, &funCode, 1);
    HToAChar(pwBuf + 5, (uint8_t *)&byteCnt, 1);
    HToAChar(pwBuf + 7, (uint8_t *)buf, byteCnt);

    // // 补充上 LRC
    calc_lrc = ASCII_LRC(pwBuf + 1, 2 * (3 + byteCnt)); //

    LOG_D("calc_lrc:0x%02X", calc_lrc);

    HToAChar(pwBuf + 7 + 2 * byteCnt, (uint8_t *)&calc_lrc, 1);
    rt_memcpy(pwBuf + 7 + 2 * byteCnt + 2, "\r\n", 2);

    // for (int j = 0; j < frame_len; j++)
    // {
    //     rt_kprintf("%02x ", *(pwBuf + j));
    // }

    // log_d("res:%s", pwBuf);

    for (int i = 0; i < frame_len; i++)
    {
        rt_kprintf("%c", *(pwBuf + i));
    }

    struct SER_PORT *port = &g_stConfig.serPort[1];
    log_d("port:%s", port->dev_name);
    rt_device_write(port->device, 0, pwBuf, frame_len);
    free(pwBuf);
}

uint8_t parse_ascii_frame(char *ptrFrame, rt_uint32_t frame_len)
{
    log_d("开始解析 ASCII frame:%d", frame_len);

    uint8_t slaveAddr; //上位机请求中的从机地
    uint8_t funcode;   //上位机请求中的从机地址

    uint16_t ReadDatStAdd; //上位机要的读的数据起始
    int ReadDatLenth;      //上位机要读的数据的长度

    uint16_t WriteSingleData; // 06 功能码写单个寄存器的数据

    int WriteDatStAdd; //上位机要写的数据的起始地址
    int WriteDatLenth; //上位机要写的数据的长度
    int WriteByteNum;  //上位机要写的数据的字节长度=数据长度*2

    uint8_t lrc = 0; //上位机请求中的 LRC
    uint8_t calc_lrc = 0;

    rt_bool_t isValid = RT_FALSE;

    uint8_t *pWbuf;

    struct SER_PORT *port = &g_stConfig.serPort[0];

    int i = 0, j = 0;

    // for (i = 0; i < frame_len; i++)
    // {
    //     rt_kprintf("%02x ", *(ptrFrame + i));
    // }
    // rt_kprintf("\n");

    for (i = 0; i < frame_len; i++)
    {
        // 查找帧头
        if (*(ptrFrame + i) != ':')
        {
            continue;
        }
        j += 1;
        slaveAddr = ATOHChar(ptrFrame + i + j);
        j += 2;
        log_d("slaveAddr:%d", slaveAddr);

        // if (slaveAddr != port->slaveAddr)
        // {
        //     LOG_W("Please check the slave addr");
        //     continue;
        // }

        funcode = ATOHChar(ptrFrame + i + j);
        j += 2;
        LOG_D("funcode:0x%02x", funcode);

        // if (funcode != 0x17) //功能代码符合0x17
        // {
        //     LOG_W("Only support 0x17 but got 0x%02x", funcode);
        //     continue;
        // }

        if (funcode == 0x03 || funcode == 0x17)
        {
            ReadDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            LOG_D("ReadDatStAdd:%04x", ReadDatStAdd);

            //功能代码23（0x17)要读取的数据起始地址，占4个字节[3]~[6]
            ReadDatLenth = ATOHInt(ptrFrame + i + j);
            j += 4;
            LOG_D("ReadDatLenth:%04x", ReadDatLenth);

            //功能代码23（0x17)要读取的数据长度，占4个字节[7]~[10]
            if (!(ReadDatStAdd >= RAddLimitMin && ReadDatStAdd <= RAddLimitMax && ReadDatStAdd + ReadDatLenth >= RAddLimitMin && ReadDatStAdd + ReadDatLenth <= RAddLimitMax + 1)) //读的地址和长度符合
            //起始地址不能超范围，起始地址+数据长度不能超范围
            {
                LOG_W("Invalid read parameters");
                continue;
            }
        }

        if (funcode == 0x06)
        {
            WriteDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            LOG_D("WriteDatStAdd:%04x", WriteDatStAdd); //功能代码23（0x17)要写取的数据起始地址，占4个字节[11]~[14]

            WriteSingleData = ATOHInt(ptrFrame + i + j);
            j += 4;
            LOG_D("WriteSingleData:%04x", WriteSingleData); //功能代码23（0x17)要写取的数据起始地址，占4个字节[11]~[14]
        }

        if (funcode == 0x17)
        {
            WriteDatStAdd = ATOHInt(ptrFrame + i + j);
            j += 4;
            LOG_D("WriteDatStAdd:%04x", WriteDatStAdd); //功能代码23（0x17)要写取的数据起始地址，占4个字节[11]~[14]

            WriteDatLenth = ATOHInt(ptrFrame + i + j); //功能代码23（0x17)要写取的寄存器数量，占4个字节[15]~[18]
            j += 4;
            LOG_D("WriteDatLenth:%04x", WriteDatLenth);

            WriteByteNum = ATOHChar(ptrFrame + i + j); //功能代码23（0x17)要写取的数据字节个数，占2个字节[19]~[20]
            j += 2;
            LOG_D("WriteByteNum:%02x", WriteByteNum);

            if (!(WriteByteNum == WriteDatLenth * 2 && (WriteDatLenth == 0 || (WriteDatLenth > 0 && WriteDatStAdd >= WAddLimitMin && WriteDatStAdd <= WAddLimitMax && WriteDatStAdd + WriteDatLenth >= WAddLimitMin + 1 && WriteDatStAdd + WriteDatLenth <= WAddLimitMax + 1)))) //写的地址和长度符合
            {
                //写的数据的起始地址不能超范围，写的数据起始地址+数据长度不能超范围
                LOG_W("Invalid write parameters");
                continue;
            }

            pWbuf = rt_malloc(WriteByteNum);
            for (int k = 0; k < WriteByteNum; k++)
            {
                int temp = ATOHChar(ptrFrame + i + j + 2 * k);
                rt_kprintf("temp:%02x ", temp);
                *(pWbuf + k) = temp;
                rt_kprintf("pWbuf:%02x ", *(pWbuf + k));
                if (k == WriteByteNum)
                    rt_kprintf("\n");
            }
            j += WriteByteNum * 2; //  注意这里是 * 2 ，不是 *4，长度 4 个字节，ASCII 的表示是翻倍的，占用 4*2 个 字节。
        }

        calc_lrc = ASCII_LRC(ptrFrame + 1, j - 1); // j-1 表示不计算 ：
        LOG_D("calc_lrc:%02X %d %d", calc_lrc, i, j);

        // for(int n = 0;n<j+2;n++)
        // {
        //     LOG_D("lrc %d:%02x",n,*(ptrFrame + i + n));
        // }

        lrc = ATOHChar(ptrFrame + i + j);
        LOG_D("lrc:%02X", lrc);

        j += 2;
        if (calc_lrc != lrc || calc_lrc == -1) //LRC校验和判断 -1 表示计算失败
        {
            LOG_W("LRC mismatch!");
            continue;
        }

        if (!(*(ptrFrame + i + j) == 0x0D && *(ptrFrame + i + j + 1) == 0x0A)) //结束符0x0D,和0x0A符合
        {
            LOG_W("Not found 0x0D && 0x0A");
            continue;
        }
        isValid = RT_TRUE;
    }

    if (!isValid)
    {
        return 0;
    }

    log_d("解析完毕 Valid!");
    // NO  Code                         Name                                 Function
    // 1    03(03h)             Read holding registers                Reading multiple registers
    // 2    06(06h)             Preset single register                Writing registers
    // 3    16(10h)             Preset multiple registers             Writing multiple registers
    // 4    23(17h)             Read/write 4x registers/              Reading/writing multiple registers

    switch (funcode)
    {
    case 03:
        break;
    case 06:
        break;
    case 16:
        break;
    case 23:
        log_i("功能码:%d Reading/writing multiple registers", funcode);
        log_i("从机地址:%d", slaveAddr);
        log_i("读起始地址:%d", ReadDatStAdd);
        log_i("读寄存器数量:%d", ReadDatLenth);
        log_i("写起始地址:%d", WriteDatStAdd);
        log_i("写寄存器数量:%d", WriteDatLenth);
        log_i("写字节长度:%d", WriteByteNum);

        if (WriteByteNum)
        {
            rt_kprintf("写数据:");
            for (int i = 0; i < WriteByteNum; i++)
            {
                rt_kprintf("%02x ", pWbuf[i]);
            }
            rt_kprintf("\n");
        }
        else
        {
            log_d("不需要写数据");
        }
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

        send_rtu_frame(slaveAddr, 3, ReadDatStAdd, ReadDatLenth);

        // 16
        if (WriteDatLenth)
        {
            rt_thread_delay(100);
            // send_rtu_frame(slaveAddr, 0x10,wr);
        }

        free(pWbuf);
        break;
    default:
        log_w("Not support now!");
        break;
    }

    return 0;
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

uint8_t parse_rtu_frame(char *ptrFrame, rt_uint16_t frame_len)
{
    rt_uint16_t i = 0;
    uint8_t *pwBuf;

    struct RTU_FRAME rtu_frame;

    log_d("开始解析 RTU frame:%d", frame_len);
    for (i = 0; i < frame_len; i++)
    {
        rt_kprintf("%02X ", *(ptrFrame + i));
    }
    rt_kprintf("\n");

    rtu_frame.slaveAddr = *ptrFrame;
    rtu_frame.funCode = *(ptrFrame + 1);
    rtu_frame.byteCnt = *(ptrFrame + 2);

    log_d("slaveAddr:%d", rtu_frame.slaveAddr);
    log_d("funCode:%d", rtu_frame.funCode);
    log_d("byteCnt:%d", rtu_frame.byteCnt);

    if (rtu_frame.byteCnt % 2 != 0)
    {
        log_e("数据长度不对");
        return -1;
    }

    // rtu_frame.pBytes = rt_malloc(rtu_frame.byteCnt / 2 * sizeof(uint16_t));
    // for (int j = 0; j < rtu_frame.byteCnt; j += 2)
    // {
    //     uint16_t data_hi = *(ptrFrame + 3 + j);
    //     uint16_t data_lo = *(ptrFrame + 4 + j);
    //     // log_d("data_lo:%02x",data_lo);
    //     // log_d("data_hi:%02x",data_hi);
    //     *(rtu_frame.pBytes + j / 2) = data_hi << 8 | data_lo;
    //     log_d("j:%d v:%04X", j / 2, *(rtu_frame.pBytes + j / 2));
    // }
    uint16_t crc_lo = *(ptrFrame + 3 + rtu_frame.byteCnt);
    uint16_t crc_hi = (*(ptrFrame + 4 + rtu_frame.byteCnt) << 8) & 0xFF00;
    rtu_frame.crc = crc_hi | crc_lo;

    log_d("crc:%04X", rtu_frame.crc);

    response_ascii_frame(rtu_frame.slaveAddr, ptrFrame + 3, rtu_frame.byteCnt);
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

        char thread_name[10] = {'\0'};

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
        };

        // 如果是 ascii 端口则需要通中断方式来获取数据
        if (!rt_strcmp(g_stConfig.serPort[i].prot, "ascii"))
        {

        }
        else
        {
   

        }
    }

    return 0;

exit:
    RT_ASSERT(ret == RT_EOK);
    return ret;
}