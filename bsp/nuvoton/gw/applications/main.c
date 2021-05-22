/**************************************************************************//**
*
* @copyright (C) 2019 Nuvoton Technology Corp. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*
* Change Logs:
* Date            Author       Notes
* 2020-12-12      Wayne        First version
*
******************************************************************************/

#include <rtconfig.h>
#include <rtdevice.h>

#include "drv_sys.h"

#if defined(RT_USING_PIN)
#include <drv_gpio.h>

/* defined the LED_R pin: PB13 */
#define LED_R   NU_GET_PININDEX(NU_PA, 7)

/* defined the LED_Y pin: PB8 */
#define LED_Y   NU_GET_PININDEX(NU_PA, 8)

#define KEY_1   NU_GET_PININDEX(NU_PB, 7)


#define LED_1   NU_GET_PININDEX(NU_PD, 8)

#define LED_2   NU_GET_PININDEX(NU_PD, 9)

static uint32_t u32Key1 = KEY_1;

void nu_button_cb(void *args)
{
    uint32_t u32Key = *((uint32_t *)(args));

    switch (u32Key)
    {
    case KEY_1:
				rt_kprintf("button click\n");
        rt_pin_write(LED_Y, ~rt_pin_read(LED_Y));
        rt_hw_cpu_reset();
				break;
    }
}

#endif

#if defined(BOARD_USING_UART8_RS485)

#include <drv_uart.h>
#define NU_UART_DEVNAME "uart8"

#endif

int main(int argc, char **argv)
{
   rt_err_t ret;
  int str_len;
  char txbuf[16];

#if defined(RT_USING_PIN)

    /* set LED_R pin mode to output */
    rt_pin_mode(LED_R, PIN_MODE_OUTPUT);

    /* set LED_Y pin mode to output */
    rt_pin_mode(LED_Y, PIN_MODE_OUTPUT);
	
		 rt_pin_mode(LED_1, PIN_MODE_OUTPUT);
		 rt_pin_mode(LED_2, PIN_MODE_OUTPUT);
	
		rt_pin_mode(KEY_1, PIN_MODE_INPUT_PULLUP);
	
		rt_pin_attach_irq(KEY_1, PIN_IRQ_MODE_FALLING, nu_button_cb, &u32Key1);
	
    rt_pin_irq_enable(KEY_1, PIN_IRQ_ENABLE);
   
    rt_device_t serial = rt_device_find(NU_UART_DEVNAME);

    if (!serial)
    {
        rt_kprintf("Can't find %s. EXIT.\n", NU_UART_DEVNAME);
        goto exit_test_rs485;
    }
		
		rt_kprintf("Find %s.\n", NU_UART_DEVNAME);

    /* Interrupt RX */
    ret = rt_device_open(serial, RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(ret == RT_EOK);

       /* Nuvoton private command */
    nu_uart_set_rs485aud((struct rt_serial_device *)serial, RT_FALSE);

    rt_snprintf(&txbuf[0], sizeof(txbuf), "Hello World!\r\n");
    str_len = rt_strlen(txbuf);

    while (1)
    {
				rt_pin_write(LED_Y, PIN_HIGH);
        rt_pin_write(LED_R, PIN_HIGH);
			  rt_pin_write(LED_1, PIN_HIGH);
			 rt_pin_write(LED_2, PIN_HIGH);
        rt_thread_mdelay(500);
			 rt_pin_write(LED_Y, PIN_LOW);
        rt_pin_write(LED_R, PIN_LOW);
			rt_pin_write(LED_1, PIN_LOW);
			rt_pin_write(LED_2, PIN_LOW);
        rt_thread_mdelay(500);

        /* Say Hello */
      ret = rt_device_write(serial, 0, &txbuf[0], str_len);
      RT_ASSERT(ret == str_len);
    }

#endif

exit_test_rs485:

    ret = rt_device_close(serial);
    RT_ASSERT(ret == RT_EOK);
    return 0;
}
