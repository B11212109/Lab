/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */

#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"

#include "../inc/color_sensor_api.h"

int color_unit_test_run(void);

#define COLOR_UART_BAUD 115200U

int main(void)
{
  uint8_t i;

  system_clock_config();
  delay_init();
  uart_print_init(COLOR_UART_BAUD);

  uart_print_write("\r\nUART1 ready: TX=PA9 RX=PA10\r\n");
  uart_print_write("SYNC:");
  for (i = 0U; i < 16U; ++i)
  {
    uart_print_putc('U');
  }
  uart_print_write("\r\n");
  uart_print_write("color test start: sensor0(I2C1 PB8/PB9), sensor1(I2C2 PB10/PB11)\r\n");

  while(1)
  {
    (void)color_unit_test_run();
    delay_ms(300);
  }
}
