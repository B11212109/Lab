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
#include "usb_cdc.h"

usbd_core_type usb_core_dev;
uint8_t usb_buffer[256];

int main(void)
{
	uint16_t data_len;

  system_clock_config();

  at32_board_init();
  usb_cdc_init(&usb_core_dev);
  while(1)
  {
	  /* get usb vcp receive data */
	  data_len = usb_vcp_get_rxdata(&usb_core_dev, usb_buffer);

	  /* usb send data to host  */
	  if (data_len > 0) {
	    while (usb_vcp_send_data(&usb_core_dev, usb_buffer, data_len) != SUCCESS)
	  ;
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
