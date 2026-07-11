#include "OLED.h"

#include "at32f403a_407_board.h"
#include "at32f403a_407_crm.h"
#include "at32f403a_407_gpio.h"
#include "at32f403a_407_spi.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define OLED_PAGE_COUNT                  (OLED_HEIGHT / 8U)
#if defined(SH1106)
#define OLED_COLUMN_OFFSET               2U
#else
#define OLED_COLUMN_OFFSET               0U
#endif

#define OLED_SPI                         SPI2
#define OLED_SPI_CLK                     CRM_SPI2_PERIPH_CLOCK

#define OLED_CS_PORT                     GPIOB
#define OLED_CS_PIN                      GPIO_PINS_12
#define OLED_CS_GPIO_CLK                 CRM_GPIOB_PERIPH_CLOCK
#define OLED_SCL_PORT                    GPIOB
#define OLED_SCL_PIN                     GPIO_PINS_13
#define OLED_SCL_GPIO_CLK                CRM_GPIOB_PERIPH_CLOCK
#define OLED_DC_PORT                     GPIOB
#define OLED_DC_PIN                      GPIO_PINS_14
#define OLED_DC_GPIO_CLK                 CRM_GPIOB_PERIPH_CLOCK
#define OLED_SDA_PORT                    GPIOB
#define OLED_SDA_PIN                     GPIO_PINS_15
#define OLED_SDA_GPIO_CLK                CRM_GPIOB_PERIPH_CLOCK
#define OLED_RES_PORT                    GPIOA
#define OLED_RES_PIN                     GPIO_PINS_11
#define OLED_RES_GPIO_CLK                CRM_GPIOA_PERIPH_CLOCK

#define OLED_PI                          3.1415926f

static uint8_t OLED_DisplayBuf[OLED_PAGE_COUNT][OLED_WIDTH];
static bool g_oled_color_mode = true;

static void OLED_SetCursor(uint8_t page, uint8_t x);

static void OLED_CS_Write(uint8_t level)
{
    if (level != 0U) {
        gpio_bits_set(OLED_CS_PORT, OLED_CS_PIN);
    } else {
        gpio_bits_reset(OLED_CS_PORT, OLED_CS_PIN);
    }
}

static void OLED_DC_Write(uint8_t level)
{
    if (level != 0U) {
        gpio_bits_set(OLED_DC_PORT, OLED_DC_PIN);
    } else {
        gpio_bits_reset(OLED_DC_PORT, OLED_DC_PIN);
    }
}

static void OLED_RES_Write(uint8_t level)
{
    if (level != 0U) {
        gpio_bits_set(OLED_RES_PORT, OLED_RES_PIN);
    } else {
        gpio_bits_reset(OLED_RES_PORT, OLED_RES_PIN);
    }
}

static void OLED_GPIO_Init(void)
{
    gpio_init_type gpio_init_struct;
    spi_init_type spi_init_struct;

    crm_periph_clock_enable(OLED_CS_GPIO_CLK, TRUE);
    crm_periph_clock_enable(OLED_SCL_GPIO_CLK, TRUE);
    crm_periph_clock_enable(OLED_SDA_GPIO_CLK, TRUE);
    crm_periph_clock_enable(OLED_DC_GPIO_CLK, TRUE);
    crm_periph_clock_enable(OLED_RES_GPIO_CLK, TRUE);

    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;

    gpio_init_struct.gpio_pins = OLED_SCL_PIN;
    gpio_init(OLED_SCL_PORT, &gpio_init_struct);
    gpio_init_struct.gpio_pins = OLED_SDA_PIN;
    gpio_init(OLED_SDA_PORT, &gpio_init_struct);

    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = OLED_CS_PIN;
    gpio_init(OLED_CS_PORT, &gpio_init_struct);
    gpio_init_struct.gpio_pins = OLED_DC_PIN;
    gpio_init(OLED_DC_PORT, &gpio_init_struct);
    gpio_init_struct.gpio_pins = OLED_RES_PIN;
    gpio_init(OLED_RES_PORT, &gpio_init_struct);

    OLED_CS_Write(1U);
    OLED_DC_Write(1U);
    OLED_RES_Write(1U);

    crm_periph_clock_enable(OLED_SPI_CLK, TRUE);
    spi_default_para_init(&spi_init_struct);
    spi_init_struct.transmission_mode = SPI_TRANSMIT_HALF_DUPLEX_TX;
    spi_init_struct.master_slave_mode = SPI_MODE_MASTER;
    spi_init_struct.mclk_freq_division = SPI_MCLK_DIV_8;
    spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB;
    spi_init_struct.frame_bit_num = SPI_FRAME_8BIT;
    spi_init_struct.clock_polarity = SPI_CLOCK_POLARITY_LOW;
    spi_init_struct.clock_phase = SPI_CLOCK_PHASE_1EDGE;
    spi_init_struct.cs_mode_selection = SPI_CS_SOFTWARE_MODE;
    spi_init(OLED_SPI, &spi_init_struct);
    spi_enable(OLED_SPI, TRUE);
}

static void OLED_SPI_WriteByte(uint8_t byte)
{
    while (spi_i2s_flag_get(OLED_SPI, SPI_I2S_TDBE_FLAG) == RESET) {
    }
    spi_i2s_data_transmit(OLED_SPI, byte);
    while (spi_i2s_flag_get(OLED_SPI, SPI_I2S_BF_FLAG) != RESET) {
    }
}

void OLED_WriteCmd(uint8_t cmd)
{
    OLED_DC_Write(0U);
    OLED_CS_Write(0U);
    OLED_SPI_WriteByte(cmd);
    OLED_CS_Write(1U);
}

void OLED_WriteCommand(uint8_t cmd)
{
    OLED_WriteCmd(cmd);
}

void OLED_SetColorMode(bool colormode)
{
    g_oled_color_mode = colormode;
    OLED_WriteCmd(colormode ? 0xA6U : 0xA7U);
}

void OLED_Brightness(int16_t brightness)
{
    if (brightness < 0) {
        brightness = 0;
    }
    if (brightness > 255) {
        brightness = 255;
    }

    OLED_WriteCmd(0x81U);
    OLED_WriteCmd((uint8_t)brightness);
}

void OLED_WriteData(uint8_t *data, uint8_t count)
{
    uint8_t i;

    if (data == NULL) {
        return;
    }

    OLED_DC_Write(1U);
    OLED_CS_Write(0U);
    for (i = 0U; i < count; i++) {
        OLED_SPI_WriteByte(g_oled_color_mode ? data[i] : (uint8_t)~data[i]);
    }
    OLED_CS_Write(1U);
}

void OLED_Init(void)
{
    OLED_GPIO_Init();
    OLED_RES_Write(0U);
    delay_ms(10U);
    OLED_RES_Write(1U);
    delay_ms(100U);

    OLED_WriteCmd(0xAEU);
    OLED_WriteCmd(0x00U);
    OLED_WriteCmd(0x10U);
    OLED_WriteCmd(0x40U);
    OLED_WriteCmd(0xA1U);
    OLED_WriteCmd(0xC8U);
    OLED_WriteCmd(0xA6U);
    OLED_WriteCmd(0xA8U);
    OLED_WriteCmd(0x3FU);
    OLED_WriteCmd(0xD3U);
    OLED_WriteCmd(0x00U);
    OLED_WriteCmd(0xD5U);
    OLED_WriteCmd(0x80U);
    OLED_WriteCmd(0xD9U);
    OLED_WriteCmd(0xF1U);
    OLED_WriteCmd(0xDAU);
    OLED_WriteCmd(0x12U);
    OLED_WriteCmd(0xDBU);
    OLED_WriteCmd(0x40U);
#if defined(SH1106)
    OLED_WriteCmd(0xADU);
    OLED_WriteCmd(0x8BU);
#else
    OLED_WriteCmd(0x20U);
    OLED_WriteCmd(0x02U);
    OLED_WriteCmd(0x8DU);
    OLED_WriteCmd(0x14U);
#endif
    OLED_WriteCmd(0xA4U);
    OLED_WriteCmd(0xA6U);
    OLED_Brightness(100);
    OLED_WriteCmd(0xAFU);

    OLED_Clear();
    OLED_Update();
}

static void OLED_SetCursor(uint8_t page, uint8_t x)
{
    uint8_t column = (uint8_t)(x + OLED_COLUMN_OFFSET);

    OLED_WriteCmd((uint8_t)(0xB0U | page));
    OLED_WriteCmd((uint8_t)(0x10U | ((column & 0xF0U) >> 4U)));
    OLED_WriteCmd((uint8_t)(0x00U | (column & 0x0FU)));
}

void OLED_Update(void)
{
    uint8_t page;

    for (page = 0U; page < OLED_PAGE_COUNT; page++) {
        OLED_SetCursor(page, 0U);
        OLED_WriteData(OLED_DisplayBuf[page], OLED_WIDTH);
    }
}

void OLED_UpdateArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    uint8_t page;
    uint8_t page_end;

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    if ((uint16_t)x + width > OLED_WIDTH) {
        width = (uint8_t)(OLED_WIDTH - x);
    }
    if ((uint16_t)y + height > OLED_HEIGHT) {
        height = (uint8_t)(OLED_HEIGHT - y);
    }
    if (width == 0U || height == 0U) {
        return;
    }

    page_end = (uint8_t)((y + height - 1U) / 8U);
    for (page = (uint8_t)(y / 8U); page <= page_end; page++) {
        OLED_SetCursor(page, x);
        OLED_WriteData(&OLED_DisplayBuf[page][x], width);
    }
}

void OLED_Clear(void)
{
    memset(OLED_DisplayBuf, 0, sizeof(OLED_DisplayBuf));
}

void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    uint8_t i;
    uint8_t j;

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    if ((uint16_t)x + width > OLED_WIDTH) {
        width = (uint8_t)(OLED_WIDTH - x);
    }
    if ((uint16_t)y + height > OLED_HEIGHT) {
        height = (uint8_t)(OLED_HEIGHT - y);
    }

    for (j = y; j < (uint8_t)(y + height); j++) {
        for (i = x; i < (uint8_t)(x + width); i++) {
            OLED_DisplayBuf[j / 8U][i] &= (uint8_t)~(1U << (j % 8U));
        }
    }
}

void OLED_Reverse(void)
{
    uint8_t i;
    uint8_t page;

    for (page = 0U; page < OLED_PAGE_COUNT; page++) {
        for (i = 0U; i < OLED_WIDTH; i++) {
            OLED_DisplayBuf[page][i] ^= 0xFFU;
        }
    }
}

void OLED_ReverseArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    uint8_t i;
    uint8_t j;

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    if ((uint16_t)x + width > OLED_WIDTH) {
        width = (uint8_t)(OLED_WIDTH - x);
    }
    if ((uint16_t)y + height > OLED_HEIGHT) {
        height = (uint8_t)(OLED_HEIGHT - y);
    }

    for (j = y; j < (uint8_t)(y + height); j++) {
        for (i = x; i < (uint8_t)(x + width); i++) {
            OLED_DisplayBuf[j / 8U][i] ^= (uint8_t)(1U << (j % 8U));
        }
    }
}

static uint32_t OLED_Pow(uint32_t x, uint32_t y)
{
    uint32_t result = 1U;

    while (y-- != 0U) {
        result *= x;
    }

    return result;
}

void OLED_ShowImage(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *image)
{
    uint8_t i;
    uint8_t j;

    if (image == NULL) {
        return;
    }

    for (j = 0U; j < height; j++) {
        for (i = 0U; i < width; i++) {
            uint16_t image_index = (uint16_t)(j / 8U) * width + i;
            if ((image[image_index] & (1U << (j % 8U))) != 0U) {
                OLED_DrawPoint((uint8_t)(x + i), (uint8_t)(y + j));
            }
        }
    }
}

void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t font_size)
{
    if (ch < ' ' || ch > '~') {
        ch = '?';
    }

    if (font_size == OLED_8X16 || font_size == OLED_10X20_HALF) {
        OLED_ShowImage(x, y, 8U, 16U, OLED_F8x16[ch - ' ']);
    } else if (font_size == OLED_6X8 || font_size == OLED_7X12_HALF) {
        OLED_ShowImage(x, y, 6U, 8U, OLED_F6x8[ch - ' ']);
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, char *str, uint8_t font_size)
{
    uint8_t i = 0U;

    if (str == NULL) {
        return;
    }

    while (str[i] != '\0') {
        OLED_ShowChar((uint8_t)(x + (i * font_size)), y, str[i], font_size);
        i++;
    }
}

void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t number, uint8_t length, uint8_t font_size)
{
    uint8_t i;

    for (i = 0U; i < length; i++) {
        OLED_ShowChar((uint8_t)(x + (i * font_size)),
                      y,
                      (char)('0' + ((number / OLED_Pow(10U, length - i - 1U)) % 10U)),
                      font_size);
    }
}

void OLED_ShowSignedNum(uint8_t x, uint8_t y, int32_t number, uint8_t length, uint8_t font_size)
{
    uint32_t abs_value;

    if (number >= 0) {
        OLED_ShowChar(x, y, '+', font_size);
        abs_value = (uint32_t)number;
    } else {
        OLED_ShowChar(x, y, '-', font_size);
        abs_value = (uint32_t)(-number);
    }
    OLED_ShowNum((uint8_t)(x + font_size), y, abs_value, length, font_size);
}

void OLED_ShowHexNum(uint8_t x, uint8_t y, uint32_t number, uint8_t length, uint8_t font_size)
{
    uint8_t i;

    for (i = 0U; i < length; i++) {
        uint8_t value = (uint8_t)((number >> ((length - i - 1U) * 4U)) & 0x0FU);
        OLED_ShowChar((uint8_t)(x + (i * font_size)),
                      y,
                      (char)((value < 10U) ? ('0' + value) : ('A' + value - 10U)),
                      font_size);
    }
}

void OLED_ShowBinNum(uint8_t x, uint8_t y, uint32_t number, uint8_t length, uint8_t font_size)
{
    uint8_t i;

    for (i = 0U; i < length; i++) {
        uint8_t value = (uint8_t)((number >> (length - i - 1U)) & 0x01U);
        OLED_ShowChar((uint8_t)(x + (i * font_size)), y, (char)('0' + value), font_size);
    }
}

void OLED_ShowFloatNum(uint8_t x, uint8_t y, double number, uint8_t int_len, uint8_t frac_len, uint8_t font_size)
{
    char text[24];
    char format[8];

    snprintf(format, sizeof(format), "%%.%uf", (unsigned int)frac_len);
    snprintf(text, sizeof(text), format, number);
    (void)int_len;
    OLED_ShowString(x, y, text, font_size);
}

void OLED_ShowChinese(uint8_t x, uint8_t y, char *chinese)
{
    uint8_t p_chinese = 0U;
    uint8_t i;
    uint8_t index;
    char single_chinese[OLED_CHN_CHAR_WIDTH + 1U] = {0};

    if (chinese == NULL) {
        return;
    }

    for (i = 0U; chinese[i] != '\0'; i++) {
        single_chinese[p_chinese++] = chinese[i];
        if (p_chinese >= OLED_CHN_CHAR_WIDTH) {
            p_chinese = 0U;
            for (index = 0U; strcmp(OLED_CF16x16[index].Index, "") != 0; index++) {
                if (strcmp(OLED_CF16x16[index].Index, single_chinese) == 0) {
                    break;
                }
            }
            OLED_ShowImage(x, y, 16U, 16U, OLED_CF16x16[index].Data);
            x = (uint8_t)(x + 16U);
            memset(single_chinese, 0, sizeof(single_chinese));
        }
    }
}

void OLED_Printf(uint8_t x, uint8_t y, uint8_t font_size, char *format, ...)
{
    char text[128];
    va_list args;

    if (format == NULL) {
        return;
    }

    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    OLED_ShowString(x, y, text, font_size);
}

void OLED_DrawPoint(uint8_t x, uint8_t y)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }
    OLED_DisplayBuf[y / 8U][x] |= (uint8_t)(1U << (y % 8U));
}

uint8_t OLED_GetPoint(uint8_t x, uint8_t y)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return 0U;
    }
    return ((OLED_DisplayBuf[y / 8U][x] & (1U << (y % 8U))) != 0U) ? 1U : 0U;
}

void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    int16_t dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    int16_t dy = (y1 > y0) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    int16_t x = x0;
    int16_t y = y0;

    while (1) {
        OLED_DrawPoint((uint8_t)x, (uint8_t)y);
        if (x == x1 && y == y1) {
            break;
        }
        if ((2 * err) > -dy) {
            err -= dy;
            x += sx;
        }
        if ((2 * err) < dx) {
            err += dx;
            y += sy;
        }
    }
}

void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint8_t is_filled)
{
    uint8_t i;
    uint8_t j;

    if (width == 0U || height == 0U) {
        return;
    }

    if (is_filled != 0U) {
        for (j = y; j < (uint8_t)(y + height); j++) {
            for (i = x; i < (uint8_t)(x + width); i++) {
                OLED_DrawPoint(i, j);
            }
        }
    } else {
        OLED_DrawLine(x, y, (uint8_t)(x + width - 1U), y);
        OLED_DrawLine(x, (uint8_t)(y + height - 1U), (uint8_t)(x + width - 1U), (uint8_t)(y + height - 1U));
        OLED_DrawLine(x, y, x, (uint8_t)(y + height - 1U));
        OLED_DrawLine((uint8_t)(x + width - 1U), y, (uint8_t)(x + width - 1U), (uint8_t)(y + height - 1U));
    }
}

static int16_t OLED_TriangleEdge(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x, int16_t y)
{
    return (int16_t)(((x - x0) * (y1 - y0)) - ((y - y0) * (x1 - x0)));
}

void OLED_DrawTriangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t is_filled)
{
    uint8_t min_x;
    uint8_t max_x;
    uint8_t min_y;
    uint8_t max_y;
    uint8_t x;
    uint8_t y;

    if (is_filled == 0U) {
        OLED_DrawLine(x0, y0, x1, y1);
        OLED_DrawLine(x1, y1, x2, y2);
        OLED_DrawLine(x2, y2, x0, y0);
        return;
    }

    min_x = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
    max_x = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
    min_y = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
    max_y = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

    for (y = min_y; y <= max_y; y++) {
        for (x = min_x; x <= max_x; x++) {
            int16_t e0 = OLED_TriangleEdge(x0, y0, x1, y1, x, y);
            int16_t e1 = OLED_TriangleEdge(x1, y1, x2, y2, x, y);
            int16_t e2 = OLED_TriangleEdge(x2, y2, x0, y0, x, y);
            if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) {
                OLED_DrawPoint(x, y);
            }
        }
    }
}

void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t radius, uint8_t is_filled)
{
    int16_t px;
    int16_t py;
    int16_t r2 = (int16_t)radius * radius;
    int16_t inner = (int16_t)(radius - 1U) * (int16_t)(radius - 1U);

    for (py = -(int16_t)radius; py <= (int16_t)radius; py++) {
        for (px = -(int16_t)radius; px <= (int16_t)radius; px++) {
            int16_t d2 = (px * px) + (py * py);
            if ((is_filled != 0U && d2 <= r2) || (is_filled == 0U && d2 <= r2 && d2 >= inner)) {
                OLED_DrawPoint((uint8_t)(x + px), (uint8_t)(y + py));
            }
        }
    }
}

void OLED_DrawEllipse(uint8_t x, uint8_t y, uint8_t a, uint8_t b, uint8_t is_filled)
{
    int16_t px;
    int16_t py;
    int32_t a2 = (int32_t)a * a;
    int32_t b2 = (int32_t)b * b;
    int32_t limit = a2 * b2;

    if (a == 0U || b == 0U) {
        return;
    }

    for (py = -(int16_t)b; py <= (int16_t)b; py++) {
        for (px = -(int16_t)a; px <= (int16_t)a; px++) {
            int32_t value = ((int32_t)px * px * b2) + ((int32_t)py * py * a2);
            if ((is_filled != 0U && value <= limit) ||
                (is_filled == 0U && value <= limit && value >= (limit - ((int32_t)a + b) * 12))) {
                OLED_DrawPoint((uint8_t)(x + px), (uint8_t)(y + py));
            }
        }
    }
}

void OLED_DrawArc(uint8_t x, uint8_t y, uint8_t radius, int16_t start_angle, int16_t end_angle, uint8_t is_filled)
{
    int16_t angle;

    if (end_angle < start_angle) {
        end_angle = (int16_t)(end_angle + 360);
    }

    for (angle = start_angle; angle <= end_angle; angle++) {
        float rad = ((float)(angle % 360) * OLED_PI) / 180.0f;
        uint8_t px = (uint8_t)((int16_t)x + (int16_t)(cosf(rad) * radius));
        uint8_t py = (uint8_t)((int16_t)y - (int16_t)(sinf(rad) * radius));

        if (is_filled != 0U) {
            OLED_DrawLine(x, y, px, py);
        } else {
            OLED_DrawPoint(px, py);
        }
    }
}

void OLED_DrawRoundedRectangle(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                               uint8_t radius, uint8_t is_filled)
{
    if (width == 0U || height == 0U) {
        return;
    }

    if (radius == 0U) {
        OLED_DrawRectangle(x, y, width, height, is_filled);
        return;
    }

    if (radius > (width / 2U)) {
        radius = (uint8_t)(width / 2U);
    }
    if (radius > (height / 2U)) {
        radius = (uint8_t)(height / 2U);
    }
    if (radius == 0U) {
        OLED_DrawRectangle(x, y, width, height, is_filled);
        return;
    }

    if (is_filled != 0U) {
        OLED_DrawRectangle((uint8_t)(x + radius),
                           y,
                           (uint8_t)(width - (2U * radius)),
                           height,
                           OLED_FILLED);
        OLED_DrawRectangle(x,
                           (uint8_t)(y + radius),
                           width,
                           (uint8_t)(height - (2U * radius)),
                           OLED_FILLED);
    } else {
        OLED_DrawLine((uint8_t)(x + radius),
                      y,
                      (uint8_t)(x + width - radius - 1U),
                      y);
        OLED_DrawLine((uint8_t)(x + radius),
                      (uint8_t)(y + height - 1U),
                      (uint8_t)(x + width - radius - 1U),
                      (uint8_t)(y + height - 1U));
        OLED_DrawLine(x,
                      (uint8_t)(y + radius),
                      x,
                      (uint8_t)(y + height - radius - 1U));
        OLED_DrawLine((uint8_t)(x + width - 1U),
                      (uint8_t)(y + radius),
                      (uint8_t)(x + width - 1U),
                      (uint8_t)(y + height - radius - 1U));
    }

    OLED_DrawArc((uint8_t)(x + radius),
                 (uint8_t)(y + radius),
                 radius,
                 180,
                 270,
                 is_filled);
    OLED_DrawArc((uint8_t)(x + width - radius - 1U),
                 (uint8_t)(y + radius),
                 radius,
                 270,
                 360,
                 is_filled);
    OLED_DrawArc((uint8_t)(x + radius),
                 (uint8_t)(y + height - radius - 1U),
                 radius,
                 90,
                 180,
                 is_filled);
    OLED_DrawArc((uint8_t)(x + width - radius - 1U),
                 (uint8_t)(y + height - radius - 1U),
                 radius,
                 0,
                 90,
                 is_filled);
}
