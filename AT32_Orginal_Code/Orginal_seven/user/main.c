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

void GPIO_seven_INIT(void)
{
  gpio_init_type gpio_init_struct;
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE); // PA0-2 PA4-8: seven-segment segment clock
  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE); // PC0-PC3: digit select clock
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT; // PA0-2 PA4-8: segment output mode
  gpio_init_struct.gpio_pins = GPIO_PINS_0|GPIO_PINS_1|GPIO_PINS_2|GPIO_PINS_4|
                 GPIO_PINS_5|GPIO_PINS_6|GPIO_PINS_7|GPIO_PINS_8; // PA0-2 PA4-8: segment pins
  gpio_init(GPIOA,&gpio_init_struct); // PA0-2 PA4-8: segment output init

  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT; // PC0-PC3: digit select output mode
  gpio_init_struct.gpio_pins = GPIO_PINS_0|GPIO_PINS_1|GPIO_PINS_2|GPIO_PINS_3; // PC0-PC3: digit select pins
  gpio_init(GPIOC,&gpio_init_struct); // PC0-PC3: digit select output init
}

int main(void)
{
  system_clock_config();
  at32_board_init();
  GPIO_seven_INIT();

  while(1)
  {
	  gpio_port_write(GPIOC, ~0x000F); // PC0-PC3: enable all 4 digits
	  gpio_port_write(GPIOA, 0x0000); // PA0-2 PA4-8: display digit 8.
  }
}

/**
  * @}
  */

/**
  * @}
  */
