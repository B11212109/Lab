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

#define PWM_DUTY_MAX  100U // TMR3 PWM: duty range 0~99

uint16_t duty = 50; // TMR3 CH2/CH3/CH4: initial duty

void GPIO_PWM_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE); // PC7-PC9: TMR3 PWM GPIO clock
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX; // PC7-PC9: timer alternate function mode
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_pins = GPIO_PINS_7 | GPIO_PINS_8 | GPIO_PINS_9; // PC7: TMR3_CH2, PC8: TMR3_CH3, PC9: TMR3_CH4
  gpio_init(GPIOC, &gpio_init_struct); // PC7-PC9: PWM output init
}

void pwm_init(void)
{
  tmr_output_config_type tmr_oc_init_structure;
  crm_clocks_freq_type crm_clocks_freq_struct;

  crm_periph_clock_enable(CRM_TMR3_PERIPH_CLOCK, TRUE); // TMR3: peripheral clock enable
  crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE); // IOMUX: pin remap clock enable
  gpio_pin_remap_config(TMR3_MUX_11, TRUE); // PC6-PC9: remap TMR3_CH1~CH4 to GPIOC

  crm_clocks_freq_get(&crm_clocks_freq_struct);

  /* TMR3 PWM frequency:
     240MHz / (23999 + 1) / (99 + 1) = 100Hz */
  tmr_base_init(TMR3, 99, 23999); // TMR3: 100Hz PWM period
  tmr_cnt_dir_set(TMR3, TMR_COUNT_UP); // TMR3: count up mode

  tmr_output_default_para_init(&tmr_oc_init_structure);
  tmr_oc_init_structure.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_A; // TMR3: PWM mode A
  tmr_oc_init_structure.oc_idle_state = FALSE; // TMR3: idle state low
  tmr_oc_init_structure.oc_polarity = TMR_OUTPUT_ACTIVE_HIGH; // TMR3: active high PWM
  tmr_oc_init_structure.oc_output_state = TRUE; // TMR3: output enable

  tmr_output_channel_config(TMR3, TMR_SELECT_CHANNEL_2, &tmr_oc_init_structure); // PC7: TMR3_CH2 PWM config
  tmr_channel_value_set(TMR3, TMR_SELECT_CHANNEL_2, duty); // PC7: initial duty
  tmr_output_channel_buffer_enable(TMR3, TMR_SELECT_CHANNEL_2, TRUE); // PC7: enable preload

  tmr_output_channel_config(TMR3, TMR_SELECT_CHANNEL_3, &tmr_oc_init_structure); // PC8: TMR3_CH3 PWM config
  tmr_channel_value_set(TMR3, TMR_SELECT_CHANNEL_3, duty); // PC8: initial duty
  tmr_output_channel_buffer_enable(TMR3, TMR_SELECT_CHANNEL_3, TRUE); // PC8: enable preload

  tmr_output_channel_config(TMR3, TMR_SELECT_CHANNEL_4, &tmr_oc_init_structure); // PC9: TMR3_CH4 PWM config
  tmr_channel_value_set(TMR3, TMR_SELECT_CHANNEL_4, duty); // PC9: initial duty
  tmr_output_channel_buffer_enable(TMR3, TMR_SELECT_CHANNEL_4, TRUE); // PC9: enable preload

  tmr_period_buffer_enable(TMR3, TRUE); // TMR3: enable period preload
  tmr_interrupt_enable(TMR3, TMR_OVF_INT, TRUE); // TMR3: overflow interrupt enable

  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  nvic_irq_enable(TMR3_GLOBAL_IRQn, 0, 0); // TMR3: overflow interrupt NVIC enable

  tmr_output_enable(TMR3, TRUE); // TMR3: output enable
  tmr_counter_enable(TMR3, TRUE); // TMR3: start counter
}


void TMR3_GLOBAL_IRQHandler(void)
{
  if(tmr_flag_get(TMR3, TMR_OVF_FLAG) != RESET)
  {
    duty++;
    if(duty >= PWM_DUTY_MAX)
    {
      duty = 0;
    }

    tmr_channel_value_set(TMR3, TMR_SELECT_CHANNEL_2, duty); // PC7: update PWM duty
    tmr_channel_value_set(TMR3, TMR_SELECT_CHANNEL_3, duty); // PC8: update PWM duty
    tmr_channel_value_set(TMR3, TMR_SELECT_CHANNEL_4, duty); // PC9: update PWM duty

    tmr_flag_clear(TMR3, TMR_OVF_FLAG); // TMR3: clear overflow flag
  }
}


int main(void)
{
  system_clock_config();
  at32_board_init();
  GPIO_PWM_init();
  pwm_init();

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
