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
#include "usb_hid.h"
usbd_core_type usb_core_dev;
uint8_t word=0;

void GPIO_LED_button_INIT(void)
{
  gpio_init_type gpio_init_struct;
  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT; // PC6: button input mode
  gpio_init_struct.gpio_pins = GPIO_PINS_6; // PC6: button input
  gpio_init(GPIOC,&gpio_init_struct); // PC6: button input init
}

int main(void)
{
system_clock_config();
  at32_board_init();
  GPIO_LED_button_INIT();
  usb_hid_init(&usb_core_dev);
  uint8_t i = 0;

  while(1)
  {
	  if (gpio_input_data_bit_read(GPIOC,GPIO_PINS_6) == SET) {
	        if (usbd_connect_state_get(&usb_core_dev) == USB_CONN_STATE_CONFIGURED)
	        {
	          word='A'+i;
	          keyboard_send_string(&usb_core_dev, (uint8_t *)&word, 1);
	          keyboard_send_string(&usb_core_dev, (uint8_t *)"\n", 1);
	          while(gpio_input_data_bit_read(GPIOC,GPIO_PINS_6) == SET);
	          for(int j=0;j<=1000000;j++);
	        }
	        /* remote wakeup */
	        if (usbd_connect_state_get(&usb_core_dev) == USB_CONN_STATE_SUSPENDED &&
	            (usb_core_dev.remote_wakup == 1)) {
	          usbd_remote_wakeup(&usb_core_dev);
	        }
	  	  i=(i+1)%26;
	      }

  }
}
void USBFS_L_CAN1_RX0_IRQHandler(void) { usbd_irq_handler(&usb_core_dev); }

/**
  * @}
  */

/**
  * @}
  */
