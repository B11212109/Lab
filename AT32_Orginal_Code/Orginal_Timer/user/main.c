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
uint16_t second,half_second;

void tmr1_time_init(void)
{
  crm_clocks_freq_type crm_clocks_freq_struct;
  crm_clocks_freq_get(&crm_clocks_freq_struct);
  crm_periph_clock_enable(CRM_TMR1_PERIPH_CLOCK, TRUE);

  /* TMR1 time base setting: 240MHz / 24000 = 10000Hz
    10000 counts per second */
  tmr_base_init(TMR1, 10000-1, 240e2-1);
  tmr_cnt_dir_set(TMR1, TMR_COUNT_UP);
  //tmr_clock_source_div_set(TMR1, TMR_CLOCK_DIV1);
  tmr_channel_value_set(TMR1, TMR_SELECT_CHANNEL_1, 5000);
  tmr_output_channel_buffer_enable(TMR1, TMR_SELECT_CHANNEL_1, TRUE);
  tmr_period_buffer_enable(TMR1, TRUE);

  /* Enable TMR1 overflow and channel 1 interrupts */
  tmr_interrupt_enable(TMR1, TMR_OVF_INT, TRUE);
  tmr_interrupt_enable(TMR1, TMR_C1_INT, TRUE);

  /* NVIC priority configuration */
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  nvic_irq_enable(TMR1_OVF_TMR10_IRQn, 0, 0);
  nvic_irq_enable(TMR1_CH_IRQn, 0, 0);
  tmr_counter_enable(TMR1, TRUE);
}

/* TMR1 overflow interrupt: increment seconds */
void TMR1_OVF_TMR10_IRQHandler(void)
{
  if(tmr_flag_get(TMR1, TMR_OVF_FLAG) != RESET)
  {
	  second++;
      tmr_flag_clear(TMR1, TMR_OVF_FLAG);
  }
}

/* TMR1 channel 1 interrupt: increment half-second counter */
void TMR1_CH_IRQHandler(void)
{
  if(tmr_flag_get(TMR1, TMR_C1_FLAG) != RESET)
  {
	  half_second++;
      tmr_flag_clear(TMR1, TMR_C1_FLAG);
  }
}

int main(void)
{
  system_clock_config();
  at32_board_init();
  tmr1_time_init();

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
