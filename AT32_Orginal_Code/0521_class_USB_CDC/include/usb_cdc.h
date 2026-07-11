#/*
 * usb_cdc.h
 *
 */

#ifndef INCLUDE_USB_CDC_H_
#define INCLUDE_USB_CDC_H_

#include "usbd_core.h"
#include "usbd_int.h"
#include "cdc_class.h"
#include "cdc_desc.h"

#define usart_buffer_size 2048

void usb_usart_config(linecoding_type linecoding);
void usart_gpio_config(void);
void usart_send_data(uint8_t *send_data, uint16_t len);
uint16_t usart_receive_data(void);
void usb_clock48m_select(usb_clk48_s clk_s) ;
void usb_cdc_init(usbd_core_type* usb_core_dev);

#endif