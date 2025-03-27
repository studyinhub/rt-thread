#ifndef __BOARD_H__
#define __BOARD_H__

#include "NuMicro.h"
#include <drv_gpio.h>
// #include <drv_sys.h>

#if defined(__CC_ARM)
extern int Image$$RW_RAM1$$ZI$$Limit;
#define BOARD_HEAP_START (void *)&Image$$RW_RAM1$$ZI$$Limit
#else
extern int __bss_end;
#define BOARD_HEAP_START ((void *)&__bss_end)
#endif

#define BOARD_SDRAM_START 0x0
#define BOARD_SDRAM_SIZE 0x04000000
#define BOARD_HEAP_END ((void *)BOARD_SDRAM_SIZE)

#if defined(RT_USING_MTD_NAND)
#include <drivers/mtd_nand.h>
#define MTD_SPINAND_PARTITION_NUM 3
extern struct rt_mtd_nand_device mtd_partitions[MTD_SPINAND_PARTITION_NUM];
#endif

// 左 1 PA7                         右 1  PA11 UART8_RX
// 左 2 PA8                         右 2  PA12 UART8_TX
// 左 3 4G 信号灯，所以不受 CPU 控制   右 3  PA4  UART6 RX
// 左 4 电源灯                       右 4  PA5  UART6 TX

// 4G 模块 PC1 PC2 UART7
#define LED_RUN NU_GET_PININDEX(NU_PA, 7) // 左 1
#define LED_WRK NU_GET_PININDEX(NU_PA, 8) // 左 2

#define LED_OFF PIN_HIGH
#define LED_ON PIN_LOW

#define KEY_1 NU_GET_PININDEX(NU_PB, 7) // 16+7 23,低电平复

extern void rt_hw_board_init(void);
extern void nu_clock_init(void);
extern void nu_clock_deinit(void);
extern void nu_pin_init(void);
extern void nu_pin_deinit(void);

#endif /* BOARD_H_ */
