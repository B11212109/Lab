#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include <stdio.h>

extern __IO uint16_t adc1_ordinary_valuetab[DMA_buffer_size][2];
extern __IO uint16_t CURRENT_ARRAY[DMA_buffer_size][2];

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
void GPIO_Init(void)
{
	/* 結構變數宣告 */
	gpio_init_type  gpio_init_struct ;
	
	/* enable gpioa & gpiob clock */
	crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
	crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
	
	
	/* GPIOA output pin Configuration */
  gpio_init_struct.gpio_pins = GPIO_PINS_10 ;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(GPIOA, &gpio_init_struct);
}
void gpio_config(void)
{
  exint_init_type exint_init_struct;

  /* enable the gpioa clock */
  crm_periph_clock_enable( CRM_IOMUX_PERIPH_CLOCK, TRUE);
	crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
	gpio_exint_line_config(GPIO_PORT_SOURCE_GPIOA,GPIO_PINS_SOURCE6);

  /* set default parameter */
  exint_default_para_init(&exint_init_struct);

  /* configure the gpio */
  exint_init_struct.line_enable = TRUE;
  exint_init_struct.line_mode  = EXINT_LINE_INTERRUPUT;
  exint_init_struct.line_select = EXINT_LINE_6;
  exint_init_struct.line_polarity = EXINT_TRIGGER_FALLING_EDGE;
  exint_init(&exint_init_struct);
	
	nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  nvic_irq_enable(EXINT9_5_IRQn, 1, 0);
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
void TIM1_Init(void)
{
	/* 設定變數 */
	uint16_t timer1_period = 0;
	uint16_t channel2_pulse = 0;

	/* 結構變數宣告 */
	tmr_output_config_type tmr1_output_struct;
	crm_clocks_freq_type crm_clocks_freq_struct = {0};
	
	gpio_init_type  gpio_init_struct = {0};
  gpio_default_para_init(&gpio_init_struct);
	
	/* get system clock */
  crm_clocks_freq_get(&crm_clocks_freq_struct);
	
	/* enable tmr1/gpioa clock */
	crm_periph_clock_enable(CRM_TMR1_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);	
	
	/* timer1 output pin Configuration */
  gpio_init_struct.gpio_pins = GPIO_PINS_9;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init(GPIOA, &gpio_init_struct);

	/* compute the value to be set in arr register to generate signal frequency */
  timer1_period = FREQUECY_UPPER_LIMIT;
	
	/* compute c2dt value to generate a duty cycle at 50%  for channel 2 */
  channel2_pulse = FREQUECY_UPPER_LIMIT/2;
	
	tmr_base_init(TMR1, timer1_period, 0);
  tmr_cnt_dir_set(TMR1, TMR_COUNT_UP);

  /* channel 2 configuration in output mode */
  tmr_output_default_para_init(&tmr1_output_struct);
  tmr1_output_struct.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_B;
  tmr1_output_struct.oc_output_state = FALSE;
  tmr1_output_struct.oc_polarity = TMR_OUTPUT_ACTIVE_LOW;
  tmr1_output_struct.oc_idle_state = TRUE;
  tmr1_output_struct.occ_output_state = FALSE;
	
  /* hall interrupt enable */
  tmr_interrupt_enable(TMR1, TMR_OVF_INT, TRUE);
	tmr_interrupt_enable(TMR1, TMR_C2_INT, TRUE);

  /* tmr1 hall interrupt nvic init */
	nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  nvic_irq_enable(TMR1_OVF_TMR10_IRQn, 0, 0);
//	nvic_irq_enable(TMR1_CH_IRQn, 0, 0);
	/* TMR1 Channel 2 */
  tmr_output_channel_config(TMR1, TMR_SELECT_CHANNEL_2, &tmr1_output_struct);
  tmr_channel_value_set(TMR1, TMR_SELECT_CHANNEL_2, channel2_pulse);
}
void TIM8_Init(void)
{
	/* compute the value to be set in arr regiter to generate signal frequency at 100.0 khz */
 //timerperiod = ((crm_clocks_freq_struct.apb2_freq * 2) / 100000) - 1;

	/* 結構變數宣告 */
	tmr_output_config_type tmr_oc_init_structure;
	crm_clocks_freq_type crm_clocks_freq_struct = {0};
	
  crm_clocks_freq_get(&crm_clocks_freq_struct);
	/* tmr8 clock enable */
  crm_periph_clock_enable(CRM_TMR8_PERIPH_CLOCK, TRUE);
	
  /* tmr8 time base configuration */
  tmr_base_init(TMR8,((crm_clocks_freq_struct.apb2_freq*2)/1000000)-1,0);//1 Mhz
  tmr_cnt_dir_set(TMR8, TMR_COUNT_UP);
  tmr_clock_source_div_set(TMR8, TMR_CLOCK_DIV1);
	
	tmr_interrupt_enable(TMR8, TMR_OVF_INT,TRUE);
	
	nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  nvic_irq_enable(TMR8_OVF_TMR13_IRQn, 0, 0);
	
  tmr_output_default_para_init(&tmr_oc_init_structure);
  tmr_oc_init_structure.oc_mode = TMR_OUTPUT_CONTROL_PWM_MODE_B;
  tmr_oc_init_structure.oc_polarity = TMR_OUTPUT_ACTIVE_LOW;
	tmr_oc_init_structure.oc_output_state = TRUE;
  tmr_oc_init_structure.oc_idle_state = FALSE;
  tmr_output_channel_config(TMR8, TMR_SELECT_CHANNEL_1, &tmr_oc_init_structure);
  tmr_channel_value_set(TMR8, TMR_SELECT_CHANNEL_1, 120);	
	tmr_counter_enable(TMR8,TRUE);
	
}
/**
  * @brief DMA1C2 Initialization Function
  * @param None
  * @retval None
  */

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
void ADC_Init(void)
{	
	/* 結構變數宣告 */
	gpio_init_type gpio_initstructure;
	adc_base_config_type adc_base_struct;
  
	/* enable and set ADC CLOCK */
	crm_periph_clock_enable(CRM_ADC1_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_ADC2_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_ADC3_PERIPH_CLOCK, TRUE);
	crm_adc_clock_div_set(CRM_ADC_DIV_6);
	nvic_irq_enable(ADC1_2_IRQn, 0, 0); //中斷
	nvic_irq_enable(ADC3_IRQn, 0, 0); //中斷

	/* enable GPIOA CLOCK */
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
	/* 將GPIO A1(I FB2), A2(V FB),A3(I FB1)設定給ADC1,ADC2,ADC3使用 */
  gpio_default_para_init(&gpio_initstructure);
  gpio_initstructure.gpio_mode = GPIO_MODE_ANALOG;
  gpio_initstructure.gpio_pins = GPIO_PINS_1|GPIO_PINS_2|GPIO_PINS_3;
	gpio_init(GPIOA, &gpio_initstructure);


  /* select combine mode */
  adc_combine_mode_select(ADC_ORDINARY_SMLT_ONLY_MODE);
  adc_base_default_para_init(&adc_base_struct);
  adc_base_struct.sequence_mode = TRUE;//
  adc_base_struct.repeat_mode = FALSE;//
  adc_base_struct.data_align = ADC_RIGHT_ALIGNMENT;
  adc_base_struct.ordinary_channel_length = 1; //取幾筆
	adc_base_config(ADC1, &adc_base_struct);
	adc_base_config(ADC2, &adc_base_struct);
	adc_base_config(ADC3, &adc_base_struct);
	/* 設定ADC的Channel及Sampling time */
	adc_ordinary_channel_set(ADC1, ADC_CHANNEL_1, 1, ADC_SAMPLETIME_1_5); //A1
	adc_ordinary_channel_set(ADC2, ADC_CHANNEL_2, 1, ADC_SAMPLETIME_1_5); //A2
	adc_ordinary_channel_set(ADC3, ADC_CHANNEL_3, 1, ADC_SAMPLETIME_1_5); //A3

	/* 將ADC設定成TMR8的channel1觸發  */
	adc_ordinary_conversion_trigger_set(ADC1, ADC12_ORDINARY_TRIG_TMR8CH1 , TRUE);
//    adc_ordinary_conversion_trigger_set(ADC1, ADC12_ORDINARY_TRIG_SOFTWARE, TRUE);
	adc_ordinary_conversion_trigger_set(ADC2, ADC12_ORDINARY_TRIG_TMR8CH1, TRUE);
//    adc_ordinary_conversion_trigger_set(ADC2, ADC12_ORDINARY_TRIG_SOFTWARE, TRUE);
	adc_ordinary_conversion_trigger_set(ADC3, ADC3_ORDINARY_TRIG_TMR8CH1 , TRUE);
//    adc_ordinary_conversion_trigger_set(ADC3, ADC3_ORDINARY_TRIG_SOFTWARE, TRUE);
	adc_enable(ADC1, TRUE);
	adc_enable(ADC2, TRUE);
	adc_enable(ADC3, TRUE);
	//ADC1
	adc_calibration_init(ADC1);
  while(adc_calibration_init_status_get(ADC1));
	adc_calibration_start(ADC1);
  while(adc_calibration_status_get(ADC1)); 
  adc_calibration_init(ADC2);
  while(adc_calibration_init_status_get(ADC2));
  adc_calibration_start(ADC2);
  while(adc_calibration_status_get(ADC2));
  adc_calibration_init(ADC3);
  while(adc_calibration_init_status_get(ADC3));
  adc_calibration_start(ADC3);
  while(adc_calibration_status_get(ADC3));
}
