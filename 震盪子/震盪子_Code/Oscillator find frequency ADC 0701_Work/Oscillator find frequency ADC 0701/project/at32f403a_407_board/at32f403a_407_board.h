/**
  **************************************************************************
  * @file     at32f403a_407_board.h
  * @brief    header file for at-start board. set of firmware functions to
  *           manage leds and push-button. initialize delay function.
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

#ifndef __AT32F403A_407_BOARD_H
#define __AT32F403A_407_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "at32f403a_407.h"

/** @addtogroup AT32F403A_407_board
  * @{
  */

/** @addtogroup BOARD
  * @{
  */

/** @defgroup BOARD_pins_definition
  * @{
  */

/**
  * this header include define support list:
  * 1. at-start-f403a v1.x board
  * 2. at-start-f407 v1.x board
  * if define AT_START_F403A_V1, the header file support at-start-f403a v1.x board
  * if define AT_START_F407_V1, the header file support at-start-f407 v1.x board
  */

#if !defined (AT_START_F403A_V1)&& !defined (AT_START_F407_V1)
#error "please select first the board at-start device used in your application (in at32f403a_407_board.h file)"
#endif



/**************** define print uart ******************/
#define PRINT_UART                       USART3
#define PRINT_UART_CRM_CLK               CRM_USART3_PERIPH_CLOCK
#define PRINT_UART_TX_PIN                GPIO_PINS_10
#define PRINT_UART_RX_PIN                GPIO_PINS_11
#define PRINT_UART_TX_GPIO               GPIOB
#define PRINT_UART_TX_GPIO_CRM_CLK       CRM_GPIOB_PERIPH_CLOCK

/**
  * @}
  */

/** @defgroup BOARD_exported_functions
  * @{
  */

/******************** functions ********************/

/* delay function */
void delay_init(void);
void delay_us(uint32_t nus);
void delay_ms(uint16_t nms);
void delay_sec(uint16_t sec);

/* printf uart init function */
void uart_print_init(uint32_t baudrate);
void gpio_config(void);
/* periphral initial	*/
void GPIO_Init(void);
void TIM1_Init(void);
void TIM8_Init(void);
void ADC_Init(void);
/**
  * @}
  */
void PWM_FREQ_SET(uint32_t value, float DUTY);
void TMR1_Start(void);
void TMR1_Stop(void);
void sweep1(void);
void sweep2(void);
void TMR8_Start(void);
void TMR8_Stop(void);
void SHUTDOWN_PWM(void);
void TURN_ON_PWM(void);
void track_sweep(void);
void power_calculate(void);
void power_calculate_2(void);
void lowpass_Fir_filter(void);
static double bessel0(double x);
static double keiser(int i, int n, double beta);
static double window(int type, int n, int i, double beta)	;
void Firwin(int n, int band, double fln, double fhn, int wn, double* h);
/** @
  * @
  */
	
#define DMA_buffer_size  			256	//DMA的BUFFER大小

//有載震盪子工作頻率範圍

#define FREQUECY_UPPER_LIMIT 8500//28.2kHz  	掃頻的頻率上限 240M/8500=28.235

#define FREQUECY_LOWER_LIMIT		9700	//35kHz		掃頻的頻率下限 //6857 35k

#define DUTY_LOWER_LIMIT	0.15    		//Duty15%			半橋Duty下限
#define DUTY_UPPER_LIMIT	0.5				//Duty40%			半橋Duty上限


/**
  * @}
  */
	
	
#ifdef __cplusplus
}
#endif

#endif


