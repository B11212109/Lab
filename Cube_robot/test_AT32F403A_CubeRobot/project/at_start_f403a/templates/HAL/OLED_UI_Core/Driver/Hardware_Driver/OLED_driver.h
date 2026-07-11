#ifndef __OLED_DRIVER_H
#define __OLED_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "at32f403a_407_board.h"
#include "at32f403a_407_crm.h"
#include "at32f403a_407_gpio.h"
#include "at32f403a_407_spi.h"

/* Define SH1106 only when the panel uses SH1106 column offset. */
/* #define SH1106 */
void OLED_Init(void);
void OLED_Update(void);
void OLED_UpdateArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height);
void OLED_SetColorMode(bool colormode);
void OLED_Brightness(int16_t Brightness);

#endif
