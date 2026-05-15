#ifndef __MY_UTILS_H__
#define __MY_UTILS_H__

#include <stdint.h>
#include <rtthread.h>

extern char ATOHChar(char *var);
extern int ATOHInt(char *var);
extern int HToAChar(char *pDstAsc, uint8_t *pSrcHex, uint16_t len,
                    uint8_t endian);
extern int HEX_LRC(uint8_t *buf, uint16_t len);
extern int ASCII_LRC(uint8_t *buf, uint16_t len);
extern void endian_convert_int16(int16_t *buf, size_t size);
extern uint8_t calculate_lrc(const uint8_t *data, uint16_t len);
extern void uint8_to_ascii(uint8_t value, uint8_t *out);
extern uint8_t ascii_to_uint8(const uint8_t *hex);
#endif