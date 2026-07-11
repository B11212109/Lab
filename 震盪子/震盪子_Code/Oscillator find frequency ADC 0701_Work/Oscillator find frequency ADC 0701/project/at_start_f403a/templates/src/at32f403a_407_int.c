/**
  **************************************************************************
  * @file     at32f403a_407_int.c
  * @brief    main interrupt service routines.
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

/* includes ------------------------------------------------------------------*/
//#include "at32f403a_407_int.h"
#include "at32f403a_407_board.h"





extern uint16_t uart_flag;
extern uint16_t flag;
extern int seccnt;


/** @addtogroup AT32F403A_periph_template
  * @{
  */

/** @addtogroup 403A_LED_toggle
  * @{
  */

/**
  * @brief  this function handles nmi exception.
  * @param  none
  * @retval none
  */
void NMI_Handler(void)
{
}

/**
  * @brief  this function handles hard fault exception.
  * @param  none
  * @retval none
  */
void HardFault_Handler(void)
{
  /* go to infinite loop when hard fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles memory manage exception.
  * @param  none
  * @retval none
  */
void MemManage_Handler(void)
{
  /* go to infinite loop when memory manage exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles bus fault exception.
  * @param  none
  * @retval none
  */
void BusFault_Handler(void)
{
  /* go to infinite loop when bus fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles usage fault exception.
  * @param  none
  * @retval none
  */
void UsageFault_Handler(void)
{
  /* go to infinite loop when usage fault exception occurs */
  while(1)
  {
  }
}

/**
  * @brief  this function handles svcall exception.
  * @param  none
  * @retval none
  */
void SVC_Handler(void)
{
}

/**
  * @brief  this function handles debug monitor exception.
  * @param  none
  * @retval none
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  this function handles pendsv_handler exception.
  * @param  none
  * @retval none
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  this function handles systick handler.
  * @param  none
  * @retval none
  */
void SysTick_Handler(void)
{
}

/**
  * @brief  this function handles dma1_channel1 handler.
  * @param  none
  * @retval none
  */

/**
  * @brief  this function handles TMR1 overflow handler.
  * @param  none
  * @retval none
  */ 


void TMR1_OVF_TMR10_IRQHandler(void)
{
	if(tmr_flag_get(TMR1,TMR_OVF_FLAG) != RESET)//判斷TMR1的TMR_OVF_FLAG是否舉起，是則執行下列程式
	{	
		tmr_flag_clear(TMR1, TMR_OVF_FLAG);				//清除TMR1的TMR_OVF_FLAG旗標									//舉起tmr1_tr_flag旗標
	}
}

void TMR8_OVF_TMR13_IRQHandler(void)
{
	if(tmr_flag_get(TMR8,TMR_OVF_FLAG) != RESET)//判斷TMR1的TMR_OVF_FLAG是否舉起，是則執行下列程式
	{	
		tmr_flag_clear(TMR8, TMR_OVF_FLAG);				//清除TMR1的TMR_OVF_FLAG旗標									//舉起tmr1_tr_flag旗標
		flag=1;
		seccnt++;
	}
	
}


void EXINT9_5_IRQHandler(void)
{
	if(exint_flag_get(EXINT_LINE_6) != RESET)
  {
    
    exint_flag_clear(EXINT_LINE_6);
		uart_flag = 1;	
  }
	
}

/**
  * @brief  this function handles TMR1_channel2 handler.
  * @param  none
  * @retval none
  */ 
