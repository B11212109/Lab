#ifndef OLED_H
#define OLED_H

#include <stdbool.h>
#include <stdint.h>
#include "OLED_Data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Define SH1106 when the panel needs the SH1106 column offset and charge pump command. */
/* #define SH1106 */

#define OLED_WIDTH                      128U
#define OLED_HEIGHT                     64U

#define OLED_10X20_HALF                 10U
#define OLED_8X16_HALF                  8U
#define OLED_7X12_HALF                  7U
#define OLED_6X8_HALF                   6U
#define OLED_8X8_FULL                   8U
#define OLED_12X12_FULL                 12U
#define OLED_16X16_FULL                 16U
#define OLED_20X20_FULL                 20U

#define OLED_8X16                       OLED_8X16_HALF
#define OLED_6X8                        OLED_6X8_HALF

#define OLED_UNFILLED                   0U
#define OLED_FILLED                     1U

void OLED_Init(void);
void OLED_WriteCmd(uint8_t cmd);
void OLED_WriteCommand(uint8_t cmd);
void OLED_WriteData(uint8_t *data, uint8_t count);
void OLED_SetColorMode(bool colormode);
void OLED_Brightness(int16_t brightness);

void OLED_Update(void);
void OLED_UpdateArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height);

void OLED_Clear(void);
void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void OLED_Reverse(void);
void OLED_ReverseArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height);

void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t font_size);
void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t font_size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t number, uint8_t length, uint8_t font_size);
void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t number, uint8_t length, uint8_t font_size);
void OLED_ShowHexNum(uint8_t x, uint8_t y, uint32_t number, uint8_t length, uint8_t font_size);
void OLED_ShowBinNum(uint8_t x, uint8_t y, uint32_t number, uint8_t length, uint8_t font_size);
void OLED_ShowFloatNum(uint8_t x, uint8_t y, double number, uint8_t int_len, uint8_t frac_len, uint8_t font_size);
void OLED_ShowChinese(uint8_t x, uint8_t y, char *chinese);
void OLED_ShowImage(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *image);
void OLED_Printf(uint8_t x, uint8_t y, uint8_t font_size, char *format, ...);

void OLED_DrawPoint(uint8_t x, uint8_t y);
uint8_t OLED_GetPoint(uint8_t x, uint8_t y);
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t is_filled);
void OLED_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t is_filled);
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t radius, uint8_t is_filled);
void OLED_DrawEllipse(uint8_t x, uint8_t y, uint8_t a, uint8_t b, uint8_t is_filled);
void OLED_DrawArc(uint8_t x, uint8_t y, uint8_t radius, int16_t start_angle, int16_t end_angle, uint8_t is_filled);
void OLED_DrawRoundedRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t radius, uint8_t is_filled);

#ifdef __cplusplus
}
#endif

#endif
