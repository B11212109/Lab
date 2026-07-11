/*
 * usb_hid.h
 *
 */

#ifndef INCLUDE_USB_HID_H_
#define INCLUDE_USB_HID_H_

#include "usbd_core.h"
#include "usbd_int.h"
#include "keyboard_class.h"

void keyboard_send_string(void *udev, uint8_t *string, uint8_t len);
void keyboard_send_char(void *udev, uint8_t ch);

void usb_clock48m_select(usb_clk48_s clk_s);

void usb_hid_init(usbd_core_type *udev);


#endif /* INCLUDE_USB_HID_H_ */
