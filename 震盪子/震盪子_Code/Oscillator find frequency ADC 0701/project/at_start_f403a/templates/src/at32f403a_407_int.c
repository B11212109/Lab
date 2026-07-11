/**
  **************************************************************************
  * @file     at32f403a_407_int.c
  * @brief    中斷服務常式（ISR）
  **************************************************************************
  *                       Copyright notice & Disclaimer
  *
  * 本檔案為 Artery 官方 BSP 所提供之中斷服務程式範例，
  * 使用者可於 Artery 微控制器相關專案中進行開發與修改。
  *
  * 本軟體以「現況提供（AS IS）」方式釋出，不提供任何明示或暗示之保證。
  *
  **************************************************************************
  */

/* includes ------------------------------------------------------------------*/
#include "at32f403a_407_board.h"

/* 外部全域變數（來自主程式 main.c） */
extern uint16_t uart_flag;   // UART 輸出觸發旗標
extern uint16_t flag;        // ADC 取樣完成旗標
extern int seccnt;           // 系統時間計數（由 Timer 累加）

/** @addtogroup AT32F403A_periph_template
  * @{
  */

/**
  * @brief 非遮罩中斷（NMI）
  */
void NMI_Handler(void)
{
}

/**
  * @brief 硬體錯誤中斷（Hard Fault）
  * 發生致命錯誤時進入無限迴圈
  */
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief 記憶體管理錯誤中斷
  */
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief 匯流排錯誤中斷
  */
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief 使用錯誤中斷
  */
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief 系統呼叫中斷（SVC）
  */
void SVC_Handler(void)
{
}

/**
  * @brief 除錯監控中斷
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief PendSV 中斷（RTOS Context Switch 用）
  */
void PendSV_Handler(void)
{
}

/**
  * @brief 系統滴答計時器中斷（未使用）
  */
void SysTick_Handler(void)
{
}

/**
  * @brief TMR1 溢位中斷（PWM 主計時器）
  */
void TMR1_OVF_TMR10_IRQHandler(void)
{
	/* 判斷是否為 TMR1 溢位中斷 */
	if (tmr_flag_get(TMR1, TMR_OVF_FLAG) != RESET)
	{
		/* 清除 TMR1 溢位旗標 */
		tmr_flag_clear(TMR1, TMR_OVF_FLAG);
	}
}

/**
  * @brief TMR8 溢位中斷（ADC 取樣節拍計時器）
  */
void TMR8_OVF_TMR13_IRQHandler(void)
{
	/* 判斷是否為 TMR8 溢位中斷 */
	if (tmr_flag_get(TMR8, TMR_OVF_FLAG) != RESET)
	{
		/* 清除 TMR8 溢位旗標 */
		tmr_flag_clear(TMR8, TMR_OVF_FLAG);

		/* 通知主程式可進行 ADC 資料讀取 */
		flag = 1;

		/* 系統時間計數累加 */
		seccnt++;
	}
}

/**
  * @brief 外部中斷 EXINT Line6（UART 觸發用）
  */
void EXINT9_5_IRQHandler(void)
{
	if (exint_flag_get(EXINT_LINE_6) != RESET)
  {
    /* 清除 EXINT Line6 中斷旗標 */
    exint_flag_clear(EXINT_LINE_6);

		/* 通知主程式進行 UART 資料輸出 */
		uart_flag = 1;
  }
}

/**
  * @brief TMR1 Channel2 中斷（未使用，保留）
  */