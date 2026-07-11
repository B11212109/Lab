#include "OLED_driver.h"

#include <string.h>

#define OLED_WIDTH                       128U
#define OLED_HEIGHT                      64U
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

uint8_t OLED_DisplayBuf[OLED_PAGE_COUNT][OLED_WIDTH];

static bool g_oled_color_mode = true;

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

static void OLED_SPI_GPIO_Init(void)
{
    gpio_init_type gpio_init_struct;

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
}

static void OLED_SPI_Init(void)
{
    spi_init_type spi_init_struct;

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

static void OLED_Reset(void)
{
    OLED_RES_Write(0U);
    delay_ms(10U);
    OLED_RES_Write(1U);
    delay_ms(10U);
}

static void OLED_GPIO_Init(void)
{
    OLED_SPI_GPIO_Init();
    OLED_SPI_Init();
}

static void OLED_WriteCommand(uint8_t cmd)
{
    OLED_DC_Write(0U);
    OLED_CS_Write(0U);
    OLED_SPI_WriteByte(cmd);
    OLED_CS_Write(1U);
}

static void OLED_WriteDataArr(uint8_t *data, uint8_t count)
{
    OLED_DC_Write(1U);
    OLED_CS_Write(0U);
    for (uint8_t i = 0U; i < count; i++) {
        uint8_t byte = g_oled_color_mode ? data[i] : (uint8_t)~data[i];
        OLED_SPI_WriteByte(byte);
    }
    OLED_CS_Write(1U);
}

static void OLED_SetCursor(uint8_t page, uint8_t x)
{
    uint8_t column = (uint8_t)(x + OLED_COLUMN_OFFSET);

    OLED_WriteCommand((uint8_t)(0xB0U | page));
    OLED_WriteCommand((uint8_t)(0x10U | ((column & 0xF0U) >> 4U)));
    OLED_WriteCommand((uint8_t)(0x00U | (column & 0x0FU)));
}

void OLED_Update(void)
{
    for (uint8_t page = 0U; page < OLED_PAGE_COUNT; page++) {
        OLED_SetCursor(page, 0U);
        OLED_WriteDataArr(OLED_DisplayBuf[page], OLED_WIDTH);
    }
}

void OLED_UpdateArea(uint8_t X, uint8_t Y, uint8_t Width, uint8_t Height)
{
    if (X >= OLED_WIDTH || Y >= OLED_HEIGHT || Width == 0U || Height == 0U) {
        return;
    }
    if ((uint16_t)X + Width > OLED_WIDTH) {
        Width = (uint8_t)(OLED_WIDTH - X);
    }
    if ((uint16_t)Y + Height > OLED_HEIGHT) {
        Height = (uint8_t)(OLED_HEIGHT - Y);
    }

    for (uint8_t page = (uint8_t)(Y / 8U);
         page <= (uint8_t)((Y + Height - 1U) / 8U);
         page++) {
        OLED_SetCursor(page, X);
        OLED_WriteDataArr(&OLED_DisplayBuf[page][X], Width);
    }
}

void OLED_Brightness(int16_t Brightness)
{
    static int16_t last_brightness = -1;

    if (Brightness < 0) {
        Brightness = 0;
    }
    if (Brightness > 255) {
        Brightness = 255;
    }
    if (Brightness == last_brightness) {
        return;
    }

    last_brightness = Brightness;
    OLED_WriteCommand(0x81U);
    OLED_WriteCommand((uint8_t)Brightness);
}

void OLED_SetColorMode(bool colormode)
{
    g_oled_color_mode = colormode;
    OLED_WriteCommand(colormode ? 0xA6U : 0xA7U);
}

void OLED_Init(void)
{
    OLED_GPIO_Init();
    OLED_Reset();
    delay_ms(100U);

    OLED_WriteCommand(0xAEU);
    OLED_WriteCommand(0x00U);
    OLED_WriteCommand(0x10U);
    OLED_WriteCommand(0x40U);
    OLED_WriteCommand(0xA1U);
    OLED_WriteCommand(0xC8U);
    OLED_WriteCommand(0xA6U);
    OLED_WriteCommand(0xA8U);
    OLED_WriteCommand(0x3FU);
    OLED_WriteCommand(0xD3U);
    OLED_WriteCommand(0x00U);
    OLED_WriteCommand(0xD5U);
    OLED_WriteCommand(0x80U);
    OLED_WriteCommand(0xD9U);
    OLED_WriteCommand(0xF1U);
    OLED_WriteCommand(0xDAU);
    OLED_WriteCommand(0x12U);
    OLED_WriteCommand(0xDBU);
    OLED_WriteCommand(0x40U);
#if defined(SH1106)
    OLED_WriteCommand(0xADU);
    OLED_WriteCommand(0x8BU);
#else
    OLED_WriteCommand(0x20U);
    OLED_WriteCommand(0x02U);
    OLED_WriteCommand(0x8DU);
    OLED_WriteCommand(0x14U);
#endif
    OLED_WriteCommand(0xA4U);
    OLED_WriteCommand(0xA6U);
    OLED_WriteCommand(0xAFU);

    memset(OLED_DisplayBuf, 0, sizeof(OLED_DisplayBuf));
    OLED_Brightness(100);
    OLED_Update();
}
