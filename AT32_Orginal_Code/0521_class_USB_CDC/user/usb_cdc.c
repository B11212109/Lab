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

/** @addtogroup AT32F403A_periph_examples
 * @{
 */

/** @addtogroup 403A_USB_device_vcp_loopback USB_device_vcp_loopback
 * @{
 */

extern linecoding_type linecoding;

/**
 * @brief  usb 48M clock select
 * @param  clk_s:USB_CLK_HICK, USB_CLK_HEXT
 * @retval none
 */
void usb_clock48m_select(usb_clk48_s clk_s) {
  if (clk_s == USB_CLK_HICK) {
    crm_usb_clock_source_select(CRM_USB_CLOCK_SOURCE_HICK);

    /* enable the acc calibration ready interrupt */
    crm_periph_clock_enable(CRM_ACC_PERIPH_CLOCK, TRUE);

    /* update the c1\c2\c3 value */
    acc_write_c1(7980);
    acc_write_c2(8000);
    acc_write_c3(8020);

    /* open acc calibration */
    acc_calibration_mode_enable(ACC_CAL_HICKTRIM, TRUE);
  } else {
    switch (system_core_clock) {
    /* 48MHz */
    case 48000000:
      crm_usb_clock_div_set(CRM_USB_DIV_1);
      break;

    /* 72MHz */
    case 72000000:
      crm_usb_clock_div_set(CRM_USB_DIV_1_5);
      break;

    /* 96MHz */
    case 96000000:
      crm_usb_clock_div_set(CRM_USB_DIV_2);
      break;

    /* 120MHz */
    case 120000000:
      crm_usb_clock_div_set(CRM_USB_DIV_2_5);
      break;

    /* 144MHz */
    case 144000000:
      crm_usb_clock_div_set(CRM_USB_DIV_3);
      break;

    /* 168MHz */
    case 168000000:
      crm_usb_clock_div_set(CRM_USB_DIV_3_5);
      break;

    /* 192MHz */
    case 192000000:
      crm_usb_clock_div_set(CRM_USB_DIV_4);
      break;

    default:
      break;
    }
  }
}

/**
 * @brief  main function.
 * @param  none
 * @retval none
 */
void usb_cdc_init(usbd_core_type *usb_core_dev) {

  /* config nvic priority group */
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

  /* hardware usart config: usart2 */
  usb_usart_config(linecoding);

  /* select usb 48m clcok source */
  usb_clock48m_select(USB_CLK_HICK);

  /* enable usb clock */
  crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, TRUE);

  /* enable usb interrupt */
  nvic_irq_enable(USBFS_L_CAN1_RX0_IRQn, 0, 0);

  /* usb core init */
  usbd_core_init(usb_core_dev, USB, &cdc_class_handler, &cdc_desc_handler, 0);

  /* enable usb pull-up */
  usbd_connect(usb_core_dev);
}

/**
 * @brief  this function handles usart2  and linecoding config.
 * @param  linecoding: linecoding value
 * @retval none
 */
void usb_usart_config(linecoding_type linecoding) {
  usart_stop_bit_num_type usart_stop_bit;
  usart_data_bit_num_type usart_data_bit;
  usart_parity_selection_type usart_parity_select;

  /* stop bit */
  switch (linecoding.format) {
  case 0x0:
    usart_stop_bit = USART_STOP_1_BIT;
    break;
    /* to be used when transmitting and receiving data in smartcard mode */
  case 0x1:
    usart_stop_bit = USART_STOP_1_5_BIT;
    break;
  case 0x2:
    usart_stop_bit = USART_STOP_2_BIT;
    break;
  default:
    break;
  }

  /* parity */
  switch (linecoding.parity) {
  case 0x0:
    usart_parity_select = USART_PARITY_NONE;
    break;
  case 0x1:
    usart_parity_select = USART_PARITY_ODD;
    break;
  case 0x2:
    usart_parity_select = USART_PARITY_EVEN;
    break;
  /* hardware usart not support partiy for mark and space */
  case 0x3:
  case 0x4:
    break;
  default:
    break;
  }

  if (USART_PARITY_NONE == usart_parity_select) {
    /* data bits */
    switch (linecoding.data) {
    /* hardware usart not support data bits for 5/6/7 */
    case 0x5:
    case 0x6:
    case 0x7:
      break;
    case 0x8:
      usart_data_bit = USART_DATA_8BITS;
      break;
    /* hardware usart not support data bits for 16 */
    case 0x10:
      break;
    default:
      break;
    }
  } else {
    /* data bits */
    switch (linecoding.data) {
    /* hardware usart not support data bits for 5/6 */
    case 0x5:
    case 0x6:
      break;
    case 0x7:
      usart_data_bit = USART_DATA_8BITS;
      break;
    case 0x8:
      usart_data_bit = USART_DATA_9BITS;
      break;
    /* hardware usart not support data bits for 16 */
    case 0x10:
      break;
    default:
      break;
    }
  }
}

/**
 * @brief  usb delay millisecond function.
 * @param  ms: number of millisecond delay
 * @retval none
 */
void usb_delay_ms(uint32_t ms) {
  /* user can define self delay function */
  delay_ms(ms);
}

/**
 * @brief  usb delay microsecond function.
 * @param  us: number of microsecond delay
 * @retval none
 */
void usb_delay_us(uint32_t us) { delay_us(us); }

/**
 * @}
 */

/**
 * @}
 */
