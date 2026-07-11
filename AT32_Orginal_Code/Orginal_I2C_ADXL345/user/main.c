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
#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include "i2c_application.h"
#include "math.h"

#define RAD_TO_DEG (57.295779513082320876798154814105) // 180/PI
#define ADXL345_Address (0x53<<1) //Alt Address -> GND
#define POWER_CTL_R (0x2D) /*Power control register*/
#define DATA_FORMAT_R (0x31) /*data format register*/
#define DATA_STARI_ADDR (0x32) /*Start address to read the values*/
i2c_handle_type hi2cx;

typedef enum
{acc1_2g=0,
acc1_4g=1,
acc1_8g=2,
acc1_16g=3
}adx1345Parameters;
#define acc1 (acc1_2g)

float divider;
uint8_t ReadBuffer[6],WriteBuffer;
double ADXL345_X,ADXL345_Y,ADXL345_Z;
double Roll,Pitch;

#define I2Cx_SPEED                       100000
#define I2Cx_ADDRESS                     0xD0

#define TimeOut 1000

void i2c_lowlevel_init(i2c_handle_type* hi2c)
{
   gpio_init_type gpio_initstructure;

    /* i2c periph clock enable */
    crm_periph_clock_enable(CRM_I2C1_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);

    /* gpio configuration */
    gpio_initstructure.gpio_out_type       = GPIO_OUTPUT_OPEN_DRAIN;
    gpio_initstructure.gpio_pull           = GPIO_PULL_UP;
    gpio_initstructure.gpio_mode           = GPIO_MODE_MUX;
    gpio_initstructure.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;

    /* configure cs */
    gpio_initstructure.gpio_pins = GPIO_PINS_5; //cs
    gpio_init(GPIOB, &gpio_initstructure);

    /* configure i2c pins: scl */
    gpio_initstructure.gpio_pull           = GPIO_PULL_DOWN;
    gpio_initstructure.gpio_pins = GPIO_PINS_6; //SCL
    gpio_init(GPIOB, &gpio_initstructure);

    /* configure i2c pins: sda */
    gpio_initstructure.gpio_pull           = GPIO_PULL_DOWN;
    gpio_initstructure.gpio_pins = GPIO_PINS_7; //SDA
    gpio_init(GPIOB, &gpio_initstructure);

    nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);

    /* configure and enable i2c interrupt */
    nvic_irq_enable(I2C1_EVT_IRQn, 0, 0);
    nvic_irq_enable(I2C1_ERR_IRQn, 0, 0);

    i2c_init(I2C1, I2C_FSMODE_DUTY_2_1, I2Cx_SPEED);
    i2c_own_address1_set(I2C1, I2C_ADDRESS_MODE_7BIT, ADXL345_Address);
}

void I2Cx_EVT_IRQHandler(void)
{
  i2c_evt_irq_handler(&hi2cx);
}


void I2Cx_ERR_IRQHandler(void)
{
  i2c_err_irq_handler(&hi2cx);
}

int main(void)
{
  system_clock_config();
  hi2cx.i2cx = I2C1;
  i2c_config(&hi2cx);

  gpio_bits_write(GPIOB,GPIO_PINS_5,TRUE);
  WriteBuffer=0x00;
  while (i2c_memory_write(&hi2cx, I2C_MEM_ADDR_WIDIH_8, ADXL345_Address, POWER_CTL_R, &WriteBuffer,1,TimeOut)!=I2C_OK);//reset
  WriteBuffer=0x08;
  while (i2c_memory_write(&hi2cx, I2C_MEM_ADDR_WIDIH_8,ADXL345_Address, POWER_CTL_R,&WriteBuffer,1,TimeOut)!=I2C_OK) ; //measurement mode
  WriteBuffer=acc1;
  while (i2c_memory_write(&hi2cx, I2C_MEM_ADDR_WIDIH_8,ADXL345_Address, DATA_FORMAT_R,&WriteBuffer,1,TimeOut) !=I2C_OK);//Data format
  switch(acc1)
  {
case acc1_2g: divider=0.003906; /*1/256*/ break;
case acc1_4g: divider=0.0078125;/*1/128*/ break;
case acc1_8g: divider=0.01563;/*1/64*/ break;
case acc1_16g: divider=0.03125; /*1/32*/ break;
      }


  while(1)
  {
  while (i2c_memory_read(&hi2cx, I2C_MEM_ADDR_WIDIH_8,ADXL345_Address,DATA_STARI_ADDR,ReadBuffer,6,TimeOut)!=I2C_OK);//Data Receive
  ADXL345_X=((int16_t)(ReadBuffer[1]<<8) |ReadBuffer[0])*divider;
  ADXL345_Y=((int16_t)(ReadBuffer[3]<<8)|ReadBuffer[2])*divider;
  ADXL345_Z=( (int16_t)(ReadBuffer[5]<<8) |ReadBuffer[4])*divider;
  Roll=atan (ADXL345_Y/sqrt (pow(ADXL345_X,2)+pow(ADXL345_Z,2)))* RAD_TO_DEG;
  Pitch=atan (-ADXL345_X/sqrt (pow (ADXL345_Y,2) +pow (ADXL345_Z,2)))* RAD_TO_DEG;
  }
}

/**
  * @}
  */

/**
  * @}
  */
