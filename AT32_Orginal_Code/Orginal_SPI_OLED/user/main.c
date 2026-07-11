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

#define OLED_SPI_TIMEOUT  10000U // SPI2: transmit and receive flag timeout
#define OLED_PAGE_MAX     8U     // OLED: 8 pages x 8 pixels = 64 rows
#define OLED_COLUMN_MAX   128U   // OLED: 128 columns

extern const unsigned char font_5x7[92][5];

static void spi_configuration(void)
{
  spi_init_type spi_init_struct;

  crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, TRUE); // SPI2: peripheral clock enable
  spi_default_para_init(&spi_init_struct);
  spi_init_struct.transmission_mode = SPI_TRANSMIT_FULL_DUPLEX; // SPI2: full-duplex transmit mode
  spi_init_struct.mclk_freq_division = SPI_MCLK_DIV_16; // SPI2: baud rate divider
  spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB; // SPI2: MSB first
  spi_init_struct.frame_bit_num = SPI_FRAME_8BIT; // SPI2: 8-bit frame
  spi_init_struct.clock_polarity = SPI_CLOCK_POLARITY_HIGH; // SPI2: clock idle high
  spi_init_struct.clock_phase = SPI_CLOCK_PHASE_1EDGE; // SPI2: first edge sampling
  spi_init_struct.cs_mode_selection = SPI_CS_SOFTWARE_MODE; // PB12: software CS control
  spi_init_struct.master_slave_mode = SPI_MODE_MASTER; // SPI2: master mode

  spi_init(SPI2, &spi_init_struct);
  spi_enable(SPI2, TRUE); // SPI2: enable SPI peripheral
}

static void gpio_configuration(void)
{
  gpio_init_type gpio_initstructure;

  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE); // PB10-PB15: OLED SPI GPIO clock
  gpio_default_para_init(&gpio_initstructure);

  gpio_initstructure.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_initstructure.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_initstructure.gpio_pull = GPIO_PULL_DOWN;
  gpio_initstructure.gpio_mode = GPIO_MODE_MUX;
  gpio_initstructure.gpio_pins = GPIO_PINS_13; // PB13: SPI2_SCK pin
  gpio_init(GPIOB, &gpio_initstructure); // PB13: SPI2_SCK init

  gpio_initstructure.gpio_mode = GPIO_MODE_INPUT;
  gpio_initstructure.gpio_pins = GPIO_PINS_14; // PB14: SPI2_MISO pin
  gpio_init(GPIOB, &gpio_initstructure); // PB14: SPI2_MISO init

  gpio_initstructure.gpio_pull = GPIO_PULL_UP;
  gpio_initstructure.gpio_mode = GPIO_MODE_MUX;
  gpio_initstructure.gpio_pins = GPIO_PINS_15; // PB15: SPI2_MOSI pin
  gpio_init(GPIOB, &gpio_initstructure); // PB15: SPI2_MOSI init

  gpio_initstructure.gpio_pull = GPIO_PULL_DOWN;
  gpio_initstructure.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_initstructure.gpio_pins = GPIO_PINS_12; // PB12: OLED_CS pin
  gpio_init(GPIOB, &gpio_initstructure); // PB12: OLED_CS init
  gpio_bits_write(GPIOB, GPIO_PINS_12, TRUE); // PB12: OLED_CS inactive high

  gpio_initstructure.gpio_pins = GPIO_PINS_11; // PB11: OLED_DC pin
  gpio_init(GPIOB, &gpio_initstructure); // PB11: OLED_DC init
  gpio_bits_write(GPIOB, GPIO_PINS_11, TRUE); // PB11: OLED_DC data mode

  gpio_initstructure.gpio_pins = GPIO_PINS_10; // PB10: OLED_RESET pin
  gpio_init(GPIOB, &gpio_initstructure); // PB10: OLED_RESET init
  gpio_bits_write(GPIOB, GPIO_PINS_10, TRUE); // PB10: OLED_RESET inactive high
}

static error_status oled_spi_send_byte(uint8_t data)
{
  uint32_t timeout;

  timeout = OLED_SPI_TIMEOUT;
  while(spi_i2s_flag_get(SPI2, SPI_I2S_TDBE_FLAG) == RESET)
  {
    if(timeout == 0U)
    {
      return ERROR;
    }
    timeout--;
  }

  spi_i2s_data_transmit(SPI2, data);

  timeout = OLED_SPI_TIMEOUT;
  while(spi_i2s_flag_get(SPI2, SPI_I2S_RDBF_FLAG) == RESET)
  {
    if(timeout == 0U)
    {
      return ERROR;
    }
    timeout--;
  }

  (void)spi_i2s_data_receive(SPI2);
  return SUCCESS;
}

static error_status OLED_SendCommand(uint8_t cmd)
{
  error_status status;

  gpio_bits_write(GPIOB, GPIO_PINS_11, FALSE); // PB11: OLED_DC command mode
  gpio_bits_write(GPIOB, GPIO_PINS_12, FALSE); // PB12: OLED_CS active low
  status = oled_spi_send_byte(cmd);
  gpio_bits_write(GPIOB, GPIO_PINS_12, TRUE); // PB12: OLED_CS inactive high

  return status;
}

static error_status OLED_SendData(uint8_t data)
{
  error_status status;

  gpio_bits_write(GPIOB, GPIO_PINS_11, TRUE); // PB11: OLED_DC data mode
  gpio_bits_write(GPIOB, GPIO_PINS_12, FALSE); // PB12: OLED_CS active low
  status = oled_spi_send_byte(data);
  gpio_bits_write(GPIOB, GPIO_PINS_12, TRUE); // PB12: OLED_CS inactive high

  return status;
}

static void OLED_INIT(void)
{
  gpio_bits_write(GPIOB, GPIO_PINS_10, FALSE); // PB10: OLED_RESET active low
  delay_ms(10);
  gpio_bits_write(GPIOB, GPIO_PINS_10, TRUE); // PB10: OLED_RESET inactive high
  delay_ms(10);

  OLED_SendCommand(0xAE); // OLED: display off
  OLED_SendCommand(0x40); // OLED: display start line 0
  OLED_SendCommand(0xA1); // OLED: segment remap
  OLED_SendCommand(0xC8); // OLED: COM scan direction remap
  OLED_SendCommand(0xDA); // OLED: COM pins hardware config command
  OLED_SendCommand(0x12); // OLED: COM pins hardware config value
  OLED_SendCommand(0x81); // OLED: contrast command
  OLED_SendCommand(0x7F); // OLED: contrast value
  OLED_SendCommand(0xA4); // OLED: display follows RAM content
  OLED_SendCommand(0xA6); // OLED: normal display mode
  OLED_SendCommand(0x20); // OLED: memory addressing mode command
  OLED_SendCommand(0x00); // OLED: horizontal addressing mode
  OLED_SendCommand(0xD5); // OLED: display clock divide command
  OLED_SendCommand(0x80); // OLED: display clock divide value
  OLED_SendCommand(0x8D); // OLED: charge pump command
  OLED_SendCommand(0x14); // OLED: enable charge pump
  OLED_SendCommand(0xAF); // OLED: display on
}

static void OLED_SetCursor(uint8_t page, uint8_t column)
{
  if(page >= OLED_PAGE_MAX)
  {
    page = OLED_PAGE_MAX - 1U;
  }
  if(column >= OLED_COLUMN_MAX)
  {
    column = OLED_COLUMN_MAX - 1U;
  }

  OLED_SendCommand((uint8_t)(0xB0U | page)); // OLED: set page address
  OLED_SendCommand((uint8_t)(column & 0x0FU)); // OLED: set lower column address
  OLED_SendCommand((uint8_t)(0x10U | (column >> 4))); // OLED: set higher column address
}

static void OLED_ClearScreen(void)
{
  uint8_t page;
  uint8_t col;

  for(page = 0; page < OLED_PAGE_MAX; page++)
  {
    OLED_SetCursor(page, 0);
    for(col = 0; col < OLED_COLUMN_MAX; col++)
    {
      OLED_SendData(0x00);
    }
  }
}

static void OLED_WriteChar(char ch)
{
  uint8_t i;
  uint8_t index;

  if(ch < ' ' || ch > 'z')
  {
    ch = ' ';
  }

  index = (uint8_t)(ch - ' ');
  for(i = 0; i < 5U; i++)
  {
    OLED_SendData(font_5x7[index][i]);
  }
  OLED_SendData(0x00);
}
const unsigned char font_5x7[92][5] = {
                                       {0x00, 0x00, 0x00, 0x00, 0x00},  // space
                                       {0x00, 0x00, 0x4F, 0x00, 0x00},  // !
                                       {0x00, 0x07, 0x00, 0x07, 0x00},  // "
                                       {0x14, 0x7F, 0x14, 0x7F, 0x14},  // #
                                       {0x24, 0x2A, 0x7F, 0x2A, 0x12},  // $
                                       {0x23, 0x13, 0x08, 0x64, 0x62},  // %
                                       {0x36, 0x49, 0x55, 0x22, 0x50},  // &
                                       {0x00, 0x05, 0x03, 0x00, 0x00},  // '
                                       {0x00, 0x1C, 0x22, 0x41, 0x00},  // (
                                       {0x00, 0x41, 0x22, 0x1C, 0x00},  // )
                                       {0x14, 0x08, 0x3E, 0x08, 0x14},  // *
                                       {0x08, 0x08, 0x3E, 0x08, 0x08},  // +
                                       {0x00, 0x50, 0x30, 0x00, 0x00},  // ,
                                       {0x08, 0x08, 0x08, 0x08, 0x08},  // -
                                       {0x00, 0x60, 0x60, 0x00, 0x00},  // .
                                       {0x20, 0x10, 0x08, 0x04, 0x02},  // /
                                       {0x3E, 0x51, 0x49, 0x45, 0x3E},  // 0
                                       {0x00, 0x42, 0x7F, 0x40, 0x00},  // 1
                                       {0x42, 0x61, 0x51, 0x49, 0x46},  // 2
                                       {0x21, 0x41, 0x45, 0x4B, 0x31},  // 3
                                       {0x18, 0x14, 0x12, 0x7F, 0x10},  // 4
                                       {0x27, 0x49, 0x49, 0x49, 0x31},  // 5
                                       {0x3C, 0x4A, 0x49, 0x49, 0x30},  // 6
                                       {0x01, 0x71, 0x09, 0x05, 0x03},  // 7
                                       {0x36, 0x49, 0x49, 0x49, 0x36},  // 8
                                       {0x06, 0x49, 0x49, 0x29, 0x1E},  // 9
                                       {0x00, 0x36, 0x36, 0x00, 0x00},  // :
                                       {0x00, 0x56, 0x36, 0x00, 0x00},  // ;
                                       {0x08, 0x14, 0x22, 0x41, 0x00},  // <
                                       {0x14, 0x14, 0x14, 0x14, 0x14},  // =
                                       {0x00, 0x41, 0x22, 0x14, 0x08},  // >
                                       {0x02, 0x01, 0x51, 0x09, 0x06},  // ?
                                       {0x32, 0x49, 0x79, 0x41, 0x3E},  // @
                                       {0x7E, 0x11, 0x11, 0x11, 0x7E},  // A
                                       {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
                                       {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
                                       {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
                                       {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
                                       {0x7F, 0x09, 0x09, 0x09, 0x01},  // F
                                       {0x3E, 0x41, 0x49, 0x49, 0x7A},  // G
                                       {0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
                                       {0x00, 0x41, 0x7F, 0x41, 0x00},  // I
                                       {0x20, 0x40, 0x41, 0x3F, 0x01},  // J
                                       {0x7F, 0x08, 0x14, 0x22, 0x41},  // K
                                       {0x7F, 0x40, 0x40, 0x40, 0x40},  // L
                                       {0x7F, 0x02, 0x0C, 0x02, 0x7F},  // M
                                       {0x7F, 0x04, 0x08, 0x10, 0x7F},  // N
                                       {0x3E, 0x41, 0x41, 0x41, 0x3E},  // O
                                       {0x7F, 0x09, 0x09, 0x09, 0x06},  // P
                                       {0x3E, 0x41, 0x51, 0x21, 0x5E},  // Q
                                       {0x7F, 0x09, 0x19, 0x29, 0x46},  // R
                                       {0x46, 0x49, 0x49, 0x49, 0x31},  // S
                                       {0x01, 0x01, 0x7F, 0x01, 0x01},  // T
                                       {0x3F, 0x40, 0x40, 0x40, 0x3F},  // U
                                       {0x1F, 0x20, 0x40, 0x20, 0x1F},  // V
                                       {0x3F, 0x40, 0x38, 0x40, 0x3F},  // W
                                       {0x63, 0x14, 0x08, 0x14, 0x63},  // X
                                       {0x07, 0x08, 0x70, 0x08, 0x07},  // Y
                                       {0x61, 0x51, 0x49, 0x45, 0x43},  // Z
                                       {0x7F, 0x41, 0x41, 0x00, 0x00},  // [
                                       {0x02, 0x04, 0x08, 0x10, 0x20},  // forward slash
                                       {0x00, 0x41, 0x41, 0x7F, 0x00},  // ]
                                       {0x04, 0x02, 0x01, 0x02, 0x04},  // ^
                                       {0x40, 0x40, 0x40, 0x40, 0x40},  // _
                                       {0x00, 0x01, 0x02, 0x04, 0x00},  // `
                                       {0x20, 0x54, 0x54, 0x54, 0x78},  // a
                                       {0x7F, 0x48, 0x44, 0x44, 0x38},  // b
                                       {0x38, 0x44, 0x44, 0x44, 0x20},  // c
                                       {0x38, 0x44, 0x44, 0x48, 0x7F},  // d
                                       {0x38, 0x54, 0x54, 0x54, 0x18},  // e
                                       {0x08, 0x7E, 0x09, 0x01, 0x02},  // f
                                       {0x0C, 0x52, 0x52, 0x52, 0x3E},  // g
                                       {0x7F, 0x08, 0x04, 0x04, 0x78},  // h
                                       {0x00, 0x44, 0x7D, 0x40, 0x00},  // i
                                       {0x20, 0x40, 0x44, 0x3D, 0x00},  // j
                                       {0x7F, 0x10, 0x28, 0x44, 0x00},  // k
                                       {0x00, 0x41, 0x7F, 0x40, 0x00},  // l
                                       {0x7C, 0x04, 0x18, 0x04, 0x78},  // m
                                       {0x7C, 0x08, 0x04, 0x04, 0x78},  // n
                                       {0x38, 0x44, 0x44, 0x44, 0x38},  // o
                                       {0x7C, 0x14, 0x14, 0x14, 0x08},  // p
                                       {0x08, 0x14, 0x14, 0x18, 0x7C},  // q
                                       {0x7C, 0x08, 0x04, 0x04, 0x08},  // r
                                       {0x48, 0x54, 0x54, 0x54, 0x20},  // s
                                       {0x04, 0x3F, 0x44, 0x40, 0x20},  // t
                                       {0x3C, 0x40, 0x40, 0x20, 0x7C},  // u
                                       {0x1C, 0x20, 0x40, 0x20, 0x1C},  // v
                                       {0x3C, 0x40, 0x30, 0x40, 0x3C},  // w
                                       {0x44, 0x28, 0x10, 0x28, 0x44},  // x
                                       {0x0C, 0x50, 0x50, 0x50, 0x3C},  // y
                                       {0x44, 0x64, 0x54, 0x4C, 0x44},  // z
};


int main(void)
{
  system_clock_config();
  at32_board_init();
  delay_init();

  gpio_configuration();
  spi_configuration();
  OLED_INIT();
  OLED_ClearScreen();

  OLED_SetCursor(0, 0);
  OLED_WriteChar('0');

  while(1)
  {
  }
}

/**
  * @}
  */

/**
  * @}
  */
