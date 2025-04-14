#include "utils.h"

#define LOG_TAG "utils"
// #define LOG_LVL LOG_LVL_DBG
#define LOG_LVL LOG_LVL_ERROR
#include <ulog.h>

// ASCII 字符串 转8位16进制数 '12'转为0x12
char ATOHChar(char *var)
{
    char var1 = *var;
    char var2 = *(var + 1);

    // log_d("var1:%02x",var1);
    // log_d("var2:%02x",var2);

    char temp = 0;
    if (var1 >= '0' && var1 <= '9')
        temp = var1 - '0';
    else if (var1 >= 'A' && var1 <= 'F')
        temp = var1 - 'A' + 10;
    else
        temp = var1 - 'a';

    temp = temp << 4;

    if (var2 >= 0x30 && var2 <= 0x39)
        temp = temp + var2 - '0';
    else if (var2 >= 'A' && var2 <= 'F')
        temp = temp + var2 - 'A' + 10;
    else
        temp = temp + var2 - 'a' + 10;

    return temp;
}

// ASCII 字符串 转16位16进制数 '12AB'转为0x12AB
int ATOHInt(char *var)
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

void endian_convert_int16(int16_t *buf, size_t size)
{

    for (size_t i = 0; i < size; i++)
    {
        int16_t value = buf[i];
        uint8_t *byte1 = (uint8_t *)&value;
        uint8_t *byte2 = (uint8_t *)&value + 1;

        uint8_t temp = *byte1;
        *byte1 = *byte2;
        *byte2 = temp;

        buf[i] = value;
    }
}

// 12=>31,32
//  如果 endian false ,就不进行 16 进制字节顺序转换,否则需要
//

int HToAChar(char *pDstAsc, uint8_t *pSrcHex, uint16_t len, uint8_t endian)
{
    char temp = 0;
    char nibble[2]; // nibble 半字节的意思
    int i = 0, j = 0;

    uint8_t *tempBuf = rt_malloc(sizeof(uint8_t) * len);
    

    rt_memcpy(tempBuf, pSrcHex, len);
    // for (i = 0; i < len; i++)
    // {
    //     rt_kprintf("tempBuf[%d]=%d\r\n", i, tempBuf[i]);
    // }

    if (endian)
    {
        endian_convert_int16((int16_t *)tempBuf, len / 2);
    }

    char *buffer = rt_malloc(2 * len);


    for (i = 0; i < len; i++)
    {
        // log_d("i:%d tempBuf[i]:%02x", i, pSrcHex[i]);

        nibble[0] = tempBuf[i] >> 4 & 0x0F;
        nibble[1] = tempBuf[i] & 0x0F;

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

        rt_memcpy(buffer + i * 2, nibble, 2);
    }

    buffer[2 * len] = 0x00;
    rt_memcpy(pDstAsc, buffer, 2 * len);

    pDstAsc[2 * len] = 0x00;

    rt_free(buffer);
    rt_free(tempBuf);

    return 1;
}

// LRC checks the content of the message other than [:] of START and [CR][LF] of END. The sending side calculates and sets. The receiving side calculates based on the received message, and compares the calculation result with the received LRC. The received message is deleted if the calculation result and received LRC do not match.
// Add up the byte number of the message consisting of 8 consecutive bits. The result except the carry (overflow) is converted to 2’s complement.
// http://www.ip33.com/lrc.html
// 如果内容本身是已经 ASCII 转换过后的 buf
// 计算从 ： 之后的所有数据
int HEX_LRC(uint8_t *buf, uint16_t len)
{
    int result = 0;
    // log_d("HEX_LRC:");
    for (int i = 0; i < len; i++)
    {
        // rt_kprintf("%02x ", *(buf + i));
        result += *(buf + i);
    }
    // rt_kprintf("\n");

    return 256 - (result % 256);
}

// 如果是对 ASCII buf 直接进行计算，则需要先转换为HEX，然后再计算
int ASCII_LRC(uint8_t *buf, uint16_t len)
{
    int result = 0;
    // 先要对 ASCII 转换为 16 进制
    // 必须是 2 的倍数

    // log_d("ASCII_LRC(%d):", len);

    // for (int j = 0; j < len; j++)
    // {
    //     rt_kprintf("%02x ", *(buf + j));
    // }
    // rt_kprintf("\n");

    log_hex("ascc lrc", 16, buf, len);

    if (len % 2 != 0)
    {
        LOG_W("len is not mod by 2,please check");
        return -1;
    }

    uint8_t *hBuf;
    uint16_t hLen = len / 2; // len：整个 frame -1(:) - 4(lrc),注意 lrc 一个字节，但是是两个字符

    // log_d("hLen:%d", hLen);

    hBuf = rt_malloc(hLen);
    rt_memset(hBuf, 0, hLen);

    for (int i = 0; i < hLen; i++)
    {
        *(hBuf + i) = ATOHChar(buf + i * 2);

        // rt_kprintf("%02x ", *(hBuf + i));
    }
    // rt_kprintf("\n");

    result = HEX_LRC(hBuf, len / 2);

    rt_free(hBuf);

    return result;
}
