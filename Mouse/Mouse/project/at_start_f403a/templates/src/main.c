#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include <stdio.h>
#include <stdint.h>

/* ========== HX711 腳位定義 (GPIOB) ========== */
#define HX711_GPIO_PORT   GPIOB

#define HX711_1_DT_PIN    GPIO_PINS_5    /* DATA = PB5  */
#define HX711_1_SCK_PIN   GPIO_PINS_6    /* SCK  = PB6  */

#define HX711_2_DT_PIN    GPIO_PINS_8    /* DATA = PB8  */
#define HX711_2_SCK_PIN   GPIO_PINS_9    /* SCK  = PB9  */

#define HX711_3_DT_PIN    GPIO_PINS_0    /* DATA = PB0  */
#define HX711_3_SCK_PIN   GPIO_PINS_1    /* SCK  = PB1  */

#define HX711_4_DT_PIN    GPIO_PINS_10   /* DATA = PB10 */
#define HX711_4_SCK_PIN   GPIO_PINS_11   /* SCK  = PB11 */

/* 儲存 4 個通道最後一次成功讀取的全域變數 (防止讀取失敗時資料歸零) */
static int32_t val1 = 0;
volatile uint32_t sys_tick_ms = 0;
volatile uint32_t sample_pending_count = 0;
volatile uint32_t sample_miss_count = 0;

/* HX711 硬體通訊參數 */
#define HX711_SCK_DELAY_US          1U     /* SCK 腳位高低電位停留時間 (1微秒) */
#define HX711_READY_TIMEOUT_US   2000U     /* 最高等待轉換時間 (2毫秒)，防系統卡死 */

/* 80Hz 取樣節拍參數：TMR2 只做內部中斷，不輸出到任何 GPIO 腳位 */
#define SAMPLE_RATE_HZ             80U
#define SAMPLE_TIMER_TICK_HZ    10000U

/* ========== UART1 序列埠初始化 (與電腦通訊用) ========== */
static void gpio_uart1_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);

  /* PA9 = USART1_TX */
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins           = GPIO_PINS_9;
  gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
  gpio_init(GPIOA, &gpio_init_struct);

  /* PA10 = USART1_RX */
  gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins           = GPIO_PINS_10;
  gpio_init_struct.gpio_pull           = GPIO_PULL_UP;
  gpio_init(GPIOA, &gpio_init_struct);
}

/* 初始化 UART1 基本參數：115200 鮑率，8N1 格式 */
static void usart1_init(void)
{
  crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);

  usart_init(USART1, 115200, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_parity_selection_config(USART1, USART_PARITY_NONE);
  usart_transmitter_enable(USART1, TRUE);
  usart_receiver_enable(USART1, TRUE);
  usart_enable(USART1, TRUE);
}

/* ========== TMR2 80Hz 取樣節拍初始化 ========== */
static void sample_timer_80hz_init(void)
{
  crm_clocks_freq_type crm_clocks_freq_struct;
  uint32_t timer_clk_hz;
  uint32_t timer_div;
  uint32_t timer_period;

  crm_clocks_freq_get(&crm_clocks_freq_struct);
  timer_clk_hz = crm_clocks_freq_struct.ahb_freq;
  timer_div = (timer_clk_hz / SAMPLE_TIMER_TICK_HZ) - 1U;
  timer_period = (SAMPLE_TIMER_TICK_HZ / SAMPLE_RATE_HZ) - 1U;

  crm_periph_clock_enable(CRM_TMR2_PERIPH_CLOCK, TRUE);
  tmr_counter_enable(TMR2, FALSE);
  tmr_base_init(TMR2, timer_period, timer_div);
  tmr_cnt_dir_set(TMR2, TMR_COUNT_UP);
  tmr_clock_source_div_set(TMR2, TMR_CLOCK_DIV1);
  tmr_flag_clear(TMR2, TMR_OVF_FLAG);

  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  nvic_irq_enable(TMR2_GLOBAL_IRQn, 1, 0);

  tmr_interrupt_enable(TMR2, TMR_OVF_INT, TRUE);
  tmr_counter_enable(TMR2, TRUE);
}

/* ========== HX711 專用 GPIO 腳位初始化 ========== */
static void hx711_gpio_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
  gpio_default_para_init(&gpio_init_struct);

  /* 將所有 MCU 送往 HX711 的時脈線 (SCK) 設為輸出 */
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
  gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode           = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
  gpio_init_struct.gpio_pins           = HX711_1_SCK_PIN | HX711_2_SCK_PIN | HX711_3_SCK_PIN | HX711_4_SCK_PIN;
  gpio_init(HX711_GPIO_PORT, &gpio_init_struct);

  /* 啟動時先將時脈拉低歸零 */
  gpio_bits_reset(HX711_GPIO_PORT, HX711_1_SCK_PIN | HX711_2_SCK_PIN | HX711_3_SCK_PIN | HX711_4_SCK_PIN);

  /* 將所有接收 HX711 資料的線 (DT) 設為輸入，並開啟內部上拉電阻 */
  gpio_init_struct.gpio_mode           = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pull           = GPIO_PULL_UP; 
  gpio_init_struct.gpio_pins           = HX711_1_DT_PIN | HX711_2_DT_PIN | HX711_3_DT_PIN | HX711_4_DT_PIN;
  gpio_init(HX711_GPIO_PORT, &gpio_init_struct);
}

/* ========== HX711 讀取通訊協定 ========== 
 * 回傳值: 1(讀取成功) / 0(硬體無反應超時) 
 */
static int hx711_read_raw(uint16_t dt_pin, uint16_t sck_pin, int32_t *out)
{
  int32_t count = 0;
  unsigned long timeout_us = 0;
  int k;

  /* 確保 SCK 為低電位，準備開始通訊 */
  gpio_bits_reset(HX711_GPIO_PORT, sck_pin);

  /* 判斷 DT 腳位是否被拉低 (表示 HX711 轉換完畢，資料準備好了) */
  while(gpio_input_data_bit_read(HX711_GPIO_PORT, dt_pin))
  {
    /* 若等太久 (超過2ms) 就不等了，直接離開避免卡死主迴圈 */
    if(timeout_us >= HX711_READY_TIMEOUT_US)
    {
      return 0;
    }
    delay_us(1);
    timeout_us++;
  }

  /* 產生 24 個脈衝，依序讀回 24-bit 的類比轉數位資料 */
  for(k = 0; k < 24; k++)
  {
    gpio_bits_set(HX711_GPIO_PORT, sck_pin);
    delay_us(HX711_SCK_DELAY_US);

    count <<= 1; /* 資料靠左移，騰出最低位元 */
    if(gpio_input_data_bit_read(HX711_GPIO_PORT, dt_pin))
    {
      count++;   /* 在上升緣讀取 DT 狀態 */
    }

    gpio_bits_reset(HX711_GPIO_PORT, sck_pin);
    delay_us(HX711_SCK_DELAY_US);
  }

  /* 產生第 25 個脈衝，告訴 HX711 下一次繼續使用 "通道 A、128倍放大" */
  gpio_bits_set(HX711_GPIO_PORT, sck_pin);
  delay_us(HX711_SCK_DELAY_US);
  gpio_bits_reset(HX711_GPIO_PORT, sck_pin);
  delay_us(HX711_SCK_DELAY_US);

  /* 將 24-bit 資料轉換為 32-bit (正負號擴展/Sign Extension) 
   * 若第 24 個 bit 是 1 (代表負數)，則將高 8 bits 全部補 1 
   */
  if(count & 0x800000)
  {
    count |= 0xFF000000;
  }

  *out = count;
  return 1;
}

void TMR2_GLOBAL_IRQHandler(void)
{
  if(tmr_flag_get(TMR2, TMR_OVF_FLAG) != RESET)
  {
    sample_pending_count++;
    tmr_flag_clear(TMR2, TMR_OVF_FLAG);
  }
}

int main(void)
{
  /* 區域緩衝變數：若當次讀取失敗，這些變數的值會被拋棄 */
  int32_t raw1 = 0;
  uint8_t do_sample = 0;

  system_clock_config();
  delay_init();
  gpio_uart1_init();
  usart1_init();
  hx711_gpio_init();
  
  /* 開機初始化後，等待硬體電軌與晶片啟動穩定 */
  delay_ms(100);
  sample_timer_80hz_init();

  /* 主迴圈：等待 TMR2 80Hz 節拍，再抓取資料並傳送給電腦 */
  while(1)
  {
    do_sample = 0;

    if(sample_pending_count > 0U)
    {
      __disable_irq();
      if(sample_pending_count > 1U)
      {
        sample_miss_count += sample_pending_count - 1U;
      }
      if(sample_pending_count > 0U)
      {
        sample_pending_count = 0U;
        do_sample = 1U;
      }
      __enable_irq();
    }

    if(do_sample)
    {
      /* 依序獨立讀取 4 個通道，如果有讀到有效數字，才更新全域變數 val */
      if(hx711_read_raw(HX711_1_DT_PIN, HX711_1_SCK_PIN, &raw1)) val1 = raw1;

      /* 將最新讀到的 4 個數值用逗號分隔，透過 USART1 射出給 Python 接收 */
      printf("%ld\r\n", val1);
    }
  }
}
