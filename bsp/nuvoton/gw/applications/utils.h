#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <rtthread.h>

extern char ATOHChar(char *var);
extern int ATOHInt(char *var);
extern int HToAChar(char *pDstAsc, uint8_t *pSrcHex, uint8_t len);
extern int HEX_LRC(uint8_t *buf, uint8_t len);
extern int ASCII_LRC(uint8_t *buf, uint8_t len);
#endif