#include "keyboard_class.h"
#include "keyboard_desc.h"
#include "usb_hid.h"
#include "usbd_core.h"
#include "usbd_int.h"

/**
 * @brief  keyboard send char
 * @param  string: send char
 * @retval none
 */
void keyboard_send_char(void *udev, uint8_t ch) {
  uint8_t index = 0;
  usbd_core_type *pudev = (usbd_core_type *)udev;
  keyboard_type *pkeyboard = (keyboard_type *)pudev->class_handler->pdata;
  while (1) {
    if (pkeyboard->g_u8tx_completed == 1) {
      pkeyboard->g_u8tx_completed = 0;
      usb_hid_keyboard_send_char(udev, ch);
      break;
    }
  }
}

/**
 * @brief  keyboard send string
 * @param  string: send string
 * @param  len: send string length
 * @retval none
 */
void keyboard_send_string(void *udev, uint8_t *string, uint8_t len) {
  uint8_t index = 0;
  usbd_core_type *pudev = (usbd_core_type *)udev;
  keyboard_type *pkeyboard = (keyboard_type *)pudev->class_handler->pdata;
  for (index = 0; index < len; index++) {
    keyboard_send_char(udev, string[index]);
    keyboard_send_char(udev, 0x00);
  }
}

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

void usb_hid_init(usbd_core_type *udev) {
  /* config nvic priority group */
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  /* select usb 48m clcok source */
  usb_clock48m_select(USB_CLK_HICK);
  /* enable usb clock */
  crm_periph_clock_enable(CRM_USB_PERIPH_CLOCK, TRUE);

  /* enable usb interrupt */
  nvic_irq_enable(USBFS_L_CAN1_RX0_IRQn, 0, 0);
  /* usb core init */
  usbd_core_init(udev, USB, &keyboard_class_handler, &keyboard_desc_handler, 0);

  /* enable usb pull-up */
  usbd_connect(udev);
}
