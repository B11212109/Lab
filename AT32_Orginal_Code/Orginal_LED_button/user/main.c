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

void GPIO_LED_button_INIT(void)
{
  gpio_init_type gpio_init_struct;
  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT; // PC7/PC8/PC9: LED output mode
  gpio_init_struct.gpio_pins = GPIO_PINS_7|GPIO_PINS_8|GPIO_PINS_9; // PC7/PC8/PC9: LED outputs
  gpio_init(GPIOC,&gpio_init_struct); // PC7/PC8/PC9: LED output init

  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT; // PC6: button input mode
  gpio_init_struct.gpio_pins = GPIO_PINS_6; // PC6: button input
  gpio_init(GPIOC,&gpio_init_struct); // PC6: button input init
}

int main(void)
{
  system_clock_config();
  at32_board_init();
  GPIO_LED_button_INIT();

  while(1)
  {
    if(gpio_input_data_bit_read(GPIOC,GPIO_PINS_6)==SET) // PC6: button pressed state
	  {
      gpio_bits_write(GPIOC,GPIO_PINS_7,FALSE); // PC7: LED off
      gpio_bits_write(GPIOC,GPIO_PINS_8,FALSE); // PC8: LED off
      gpio_bits_write(GPIOC,GPIO_PINS_9,FALSE); // PC9: LED off
	  }
	  else
	  {
      gpio_bits_write(GPIOC,GPIO_PINS_7,TRUE); // PC7: LED on
      gpio_bits_write(GPIOC,GPIO_PINS_8,TRUE); // PC8: LED on
      gpio_bits_write(GPIOC,GPIO_PINS_9,TRUE); // PC9: LED on
	  }
  }
}

/**
  * @}
  */

/**
  * @}
  */
