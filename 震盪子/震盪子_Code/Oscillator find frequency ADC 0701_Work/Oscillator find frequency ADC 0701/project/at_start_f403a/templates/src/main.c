/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
  **************************************************************************
  *                       Copyright notice & Disclaimer
  * （以下為原廠版權宣告，勿動）
  **************************************************************************
  */
#include "arm_math.h"                 // CMSIS-DSP（數學/濾波等）
#include "at32f403a_407_board.h"      // 開發板 BSP（GPIO/UART 等封裝）
#include "at32f403a_407_clock.h"      // 時脈設定相關
#include "flash.h"                    // Flash 相關（若有用到）
#include <stdio.h>                    // printf
#include <stdlib.h>                   // malloc/free
#include <string.h>                   // memset/memcpy
#include <math.h>                     // fabs 等

/** @addtogroup AT32F403A_periph_template
  * @{
  */

// ===========================
// 巨集定義
// ===========================
#define DELAY                            100   // 延遲常數（用途看後續）
#define FAST                             1     // 快速模式常數
#define SLOW                             4     // 慢速模式常數

// ===========================
// 全域變數（PWM / 掃頻 / ADC / 計算用）
// ===========================
uint32_t TIMER_COUNT_NOW = 0;                 // 目前 PWM 計數週期值（對應輸出頻率）
uint32_t TIMER_COUNT_sweep2_range= 0;         // sweep2 第二次掃頻範圍上限用
uint32_t CURRENT_max = 0;                     // 目前最大值（看後面用途）
uint32_t n;                                   // 每週期樣本數（TIMER_COUNT_NOW/240 之類）
uint32_t m;                                   // 計算區塊數（(600-200)/n）
uint32_t i;                                   // 通用迴圈索引

uint16_t uart_flag=0;                         // UART 觸發旗標（收到指令後印資料）
uint16_t CURRENT_NOW = 0;                     // 目前電流值（peak-to-peak 或瞬時，依後續）
uint16_t CURRENT_max_COUNT = 0;               // 對應最大功率時的 TIMER_COUNT
volatile uint16_t flag;                                // ADC 取樣完成/更新旗標（通常在中斷設）
uint16_t adc_set;                             // ADC 設定/暫存（依後續）
float Current_power;                          // 掃頻過程中目前最大功率（通道1）
float Current_power2;                         // 掃頻過程中目前最大功率（通道2）
float power_sum;                              // 功率累加（通道1）
float power_sum2;                             // 功率累加（通道2）
float power_avg;                              // 平均功率（通道1）
float power_avg2;                             // 平均功率（通道2）
float powertotle;                             // 單段瞬時功率累加（通道1）
float powertotle2;                            // 單段瞬時功率累加（通道2）
float dataoffset[3] = {0};                    // 偏移量（V/I1/I2）用
float averagepower[2] = {0};                  // 去偏移後的 V、I（通道1）
float averagepower2[2] = {0};                 // 去偏移後的 V、I（通道2）
float power_avgset[1100] = {0};               // 每次紀錄的平均功率（通道1）
float power_avgset2[1100] = {0};              // 每次紀錄的平均功率（通道2）
float scale=0.000512;                         // 通道1 功率縮放係數（ADC→實際）
float scale2=0.222;                         // 通道2 功率縮放係數（ADC→實際）
int  TIMER_COUNT_NOW_set[1100] = {0};         // 每次紀錄對應的 TIMER_COUNT
int tt;                                       // 未知用途暫存

// ===========================
// ADC 原始資料與濾波後輸出
// ===========================
float datatest[3][600] = {{0},{0},{0}};      // 3 路 ADC，各 600 點：0=V、1=I1、2=I2（依你的讀法）
double Vout[600]={0};                         // 濾波後電壓序列
double Iout[600]={0};                         // 濾波後電流序列（通道1）
double Iout2[600]={0};                        // 濾波後電流序列（通道2）

// ===========================
// IIR 濾波器係數（a/b）與長度
// ===========================
int y_cnt=0;                                  // 目前處理資料起點 index
int alen = 14;                                // a 係數長度
double a_filter[14] = {                       // IIR 分母係數 a（注意 a[0]=1）
1.0, -11.353416991577037, 59.58562261352162, -191.39754550073351,
419.79787007863723, -663.8939062277794, 778.8039192469026,
-686.1312266652211, 453.9628071876658, -222.7704344241921,
78.80816055210454, -19.03197402817707, 2.8121227098557227,
-0.19199855066799718};

int blen = 14;                                // b 係數長度
double b_filter[14] = {                       // IIR 分子係數 b
  4.143216763182969e-14, 5.38618179213786e-13, 3.231709075282716e-12,
	1.1849599942703293e-11, 2.962399985675823e-11, 5.332319974216481e-11,
	7.109759965621975e-11, 7.109759965621975e-11, 5.332319974216481e-11,
	2.962399985675823e-11, 1.1849599942703293e-11, 3.231709075282716e-12,
	5.38618179213786e-13, 4.143216763182969e-14
};

// ===========================
// 其他控制與暫存變數
// ===========================
int max_value_now;                            // 暫存最大值（目前看 track_sweep 有用到）
int windowSize = 8;                           // 視窗大小（目前未看到使用點）
int Watt=0;                                   // 功率門檻（低於此就重掃/追蹤）
int sec_30=0;                                 // 計數器（命名像 30 秒，但實際看後面 sec_30==1）
int sum = 0;                                  // 暫存總和（目前多處註解掉）
double V_offset = 0;                             // 電壓偏移累加（通道1）
double V_offset2 = 0;                            // 電壓偏移累加（通道2/備用）
double C_offset = 0;                             // 電流偏移累加（通道1）
double C_offset2 = 0;                            // 電流偏移累加（通道2）
int V_sum = 0,V_sum2=0,i_sum=0 ,i_sum2=0;     // 各種總和（目前未完整使用）
int TIMER_COUNT_OLD;                          // track_sweep 用：掃頻範圍上界
int count_uart=0;                             // 紀錄筆數（power_avgset index）
int cnttri;                                   // 未知用途
int error_message;                            // 錯誤次數（掉功率就加）
int seccnt=0;                                 // 時間累積計數（靠迴圈累加）
int cnt1,cnt2;                                // 迴圈索引（常用）
int test11,test22;                            // 測試用變數
int y=0;                                      // 通用暫存
int ststem=1;                                 // 系統狀態（1=正常，0=停止）
int cnt;                                      // track_sweep 範圍用（error_message*10）
crm_clocks_freq_type crm_clocks_freq_struct = { 0 };  // 時脈結構（取得頻率用）

// ===========================
// 印 ADC 資料（600點）
// ===========================
void print_adc_data(void)
{
    printf("--- ADC DATA START ---\r\n");      // 開頭標記
	printf("count = %d\r\n",TIMER_COUNT_NOW);	// f = 240000000 / TIMER_COUNT_NOW
    printf("V I1 I2 Vout Iout1 Iout2\r\n");                     // 欄位標題
    for(int k = 0; k < 600; k++)               // 逐點列印
    {
        // 輸出三路資料（轉成 uint16_t 方便看）
        printf("%d %d %d %.4f %.4f %.4f\r\n",
               (uint16_t)datatest[0][k],
               (uint16_t)datatest[1][k],
               (uint16_t)datatest[2][k],
								Vout[k],
								Iout[k],
								Iout2[k]);

        if (k % 10 == 0) delay_ms(1);          // 每 10 行稍微讓 UART 不卡住
    }
	printf("%3.2f %3.2f\r\n",power_avg,power_avg2); // 印出兩通道功率
    printf("--- ADC DATA END ---\r\n");        // 結尾標記
}

/**
  * @brief  main function.
  * @param  none
  * @retval none
  */
int main(void)
{
	/* Init */
  system_clock_config();                       // 系統時脈初始化（預設/主要）
	delay_init();                                // 延遲函式初始化（SysTick 之類）
	crm_clocks_freq_get(&crm_clocks_freq_struct);// 取得各匯流排時脈資訊
	uart_print_init(230400);                     // UART 初始化（230400 bps）
	delay_ms(1);                                 // 稍等穩定

	gpio_config();                               // GPIO 設定（BSP 內）
	GPIO_Init();                                 // 你自己的 GPIO 初始化（命名同時存在）
	//printf("Turn on Power:\r\n");               // 原本提示（註解）
	TIM8_Init();                                 // Timer8 初始化（可能做 ADC 觸發/flag）
	ADC_Init();                                  // ADC 初始化
	TIM1_Init();                                 // Timer1 初始化（PWM 輸出）
	TURN_ON_PWM();                               // 解除關閉腳位（使能 PWM）
	TMR1_Start();                                // 啟動 PWM Timer1

	/*  ]wWlWvWWvU  */
	TIMER_COUNT_NOW = FREQUECY_UPPER_LIMIT ;     // 初始化 PWM 週期（頻率上限）
	PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT); // 設定 PWM 頻率與 duty
	n=TIMER_COUNT_NOW/240;                       // 由 TIMER_COUNT 推算每週期點數 n
	delay_ms(5);                                 // 稍等穩定

	TMR8_Start();                                // 啟動 Timer8（讓 flag 開始更新）
	i=0;                                         // 初始化索引

	sweep1();                                    // 第一次掃頻（粗掃）
	printf("sweep1\r\n");                        // 印出完成提示

	TIMER_COUNT_NOW=CURRENT_max_COUNT;           // 將 PWM 設到最大功率點
	printf("power_sum = %3.2f %3.2f\r\n", Current_power/scale,Current_power2/scale2);
	printf("%4d %3.2f %3.2f\n" ,TIMER_COUNT_NOW,Current_power,Current_power2); // 印結果

	sweep2();                                    // 第二次掃頻（細掃）
	printf("sweep2\r\n");                        // 印出完成提示

	TMR8_Stop();                                 // 先停 Timer8（避免取樣中斷干擾）
	TIMER_COUNT_NOW=CURRENT_max_COUNT;           // 更新到 sweep2 找到的最佳點
	printf("power_sum = %3.2f %3.2f\r\n", Current_power/scale, Current_power2/scale2);
	printf("%4d %3.2f %3.2f\n" ,TIMER_COUNT_NOW,Current_power,Current_power2); // 印結果

	PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT); // 設定 PWM 到最佳點
	n=TIMER_COUNT_NOW/240;                       // 更新 n

	TMR8_Start();                                // 重新啟動 Timer8

 while(1)
 {
	 // 每次迴圈累加 seccnt（看起來在別處可能用中斷加，這裡未看到 seccnt++）
	 if(seccnt>50000000)                         // 到達某個時間門檻（類似 1 秒/10 秒）
		 {
				// 收集 600 點 ADC
				for( cnt1=0;cnt1<600;)
				{
						if(flag==1)              // flag=1 表示有新 ADC 轉換值可讀
						{
							 datatest[0][cnt1]=adc_ordinary_conversion_data_get(ADC2);  // 讀電壓（此處用 ADC2）
							 datatest[1][cnt1]=adc_ordinary_conversion_data_get(ADC1);  // 讀電流1（此處用 ADC1）
							 datatest[2][cnt1]=adc_ordinary_conversion_data_get(ADC3);  // 讀電流2（ADC3）
							 cnt1++;             // 下一點
							 flag=0;             // 清旗標，等待下一次更新
						}
				}

				// 確認收滿且系統正常
				if((cnt1==600)&&(ststem==1))
				{
						TMR8_Stop();             // 停 Timer8，避免資料在計算時被干擾
						
						n=TIMER_COUNT_NOW/240;   // 更新 n（每週期樣本數）
					  power_calculate();         // 計算通道1 功率（V 與 Iout）
						power_calculate_2();       // 計算通道2 功率（V 與 Iout2）
						print_adc_data();        // 把原始 ADC 600 點全部印出（很大量）

						y_cnt=0;                  // 重設資料 index

						power_sum=power_sum/m;    // 平均每段功率（通道1）
						//printf("power_sum = %3.2f\r\n", power_sum);
						power_avg=power_sum*scale;// 換算成實際功率（通道1）

						power_sum2=power_sum2/m;  // 平均每段功率（通道2）
						//printf("power_sum2 = %3.2f\r\n", power_sum2);
						power_avg2=power_sum2*scale2;// 換算成實際功率（通道2）

						printf("power_avg power_avg2\n");
						printf("%3.2f %3.2f\n",power_avg,power_avg2); // 印出兩通道功率

					  power_sum=0;               // 清累加
						power_sum2=0;              // 清累加

						// 若任一通道功率低於門檻 Watt，視為異常（掉功率）
						if((power_avg<Watt)||(power_avg2<Watt))
						{
							printf("%3.2f %3.2f\n",power_avgset[i],power_avgset2[i]); // 印上一筆（或某筆）紀錄
							error_message++;        // 錯誤次數 +1

							TMR8_Start();           // 先啟動 Timer8（讓 flag 有更新）
							cnt=error_message*10;   // track_sweep 掃描半徑（越錯越大）

							track_sweep();          // 追蹤掃頻（小範圍找回最佳點）
							TIMER_COUNT_NOW=CURRENT_max_COUNT; // 更新最佳點
							printf("%4d %3.2f %3.2f\n" ,TIMER_COUNT_NOW,Current_power,Current_power2); // 印出追蹤結果

							PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT); // 設定 PWM
							printf("%4d\n",error_message); // 印出錯誤次數
						}
						else
						{
							error_message=0;        // 正常就清錯誤次數
						}

						// 若錯誤累積到 6 次：改做一次 sweep2（重新細掃）
						if(error_message==6)
						{
							printf("again sweep start:\r\n");
							sweep2();
							TIMER_COUNT_NOW=CURRENT_max_COUNT;
							PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
							printf("%4d\n" ,TIMER_COUNT_NOW);
							printf("again sweep done:\r\n");
						}

						// 若錯誤累積到 7 次：停止系統（保護）
						if(error_message==7)
						{
							//printf("%4d\n",Watt);
							TMR1_Stop();            // 停 PWM
							printf("system error:\r\n");
							ststem=0;               // 系統狀態改為停止
						}

						power_sum=0;               // 再清一次（保險）
						sec_30++;                  // 計數（名為 30，但此處每次進來都 +1）

						// 每次 sec_30==1 就記錄一筆（等於每回合都記）
						if(sec_30==1)
						{
							TIMER_COUNT_NOW_set[count_uart]=TIMER_COUNT_NOW; // 記錄頻率點
							power_avgset[count_uart]=power_avg;              // 記錄通道1 功率
							power_avgset2[count_uart]=power_avg2;            // 記錄通道2 功率
							count_uart++;                                    // 筆數 +1
							sec_30=0;                                        // 清計數
						}

						TMR8_Start();              // 計算完再啟動 Timer8
				}

				seccnt=0;                         // 清時間計數

				// 若 uart_flag 被設起（例如收到命令），就把紀錄全部印出
				if (uart_flag==1)
				{
					printf("power calculate:\r\n");
					for (i =0; i <(count_uart+1); i++)
					{
						printf("%3.2f %3.2f %4d\n" ,power_avgset[i],power_avgset2[i],TIMER_COUNT_NOW_set[i]);
					}
					uart_flag=0;                   // 清旗標
				}
			}
		}
	}

/**
  * @brief Set main PWM timer and ADC sampling rate Function
  * @param None
  * @retval None
  */
void PWM_FREQ_SET(uint32_t value, float32_t DUTY)
{
	// set TMR1 period（影響 PWM 頻率）
	tmr_period_value_set(TMR1, value);

	// 設定 PWM duty（CH2 比較值 = value * DUTY）
	tmr_channel_value_set(TMR1, TMR_SELECT_CHANNEL_2, (uint32_t)(value * DUTY) );

	// 重新從 0 開始計數
	tmr_counter_value_set(TMR1, 0);
}

/**
  * @brief Start TMR1 Timer
  * @param None
  * @retval None
  */
void TMR1_Start(void)
{
	/* TMR1 Start */
	tmr_counter_value_set(TMR1,0);               // 清計數器
	tmr_counter_enable(TMR1, TRUE);              // 開啟計數
  tmr_channel_enable(TMR1, TMR_SELECT_CHANNEL_2, TRUE); // 開啟 CH2
  tmr_output_enable(TMR1, TRUE);               // 允許輸出
}

/**
  * @brief Stop TMR1 Timer
  * @param None
  * @retval None
  */
void TMR1_Stop(void)
{
	/* TMR1 Stop */
	tmr_counter_enable(TMR1, FALSE);             // 停止計數
  tmr_channel_enable(TMR1, TMR_SELECT_CHANNEL_2, FALSE); // 關閉 CH2
  tmr_output_enable(TMR1, FALSE);              // 關閉輸出
}

// ===========================
// sweep1：粗掃（步進 5）
// ===========================
void sweep1(void)
{
	Current_power=0;                             // 清最大功率（通道1）
	Current_power2=0;                            // 清最大功率（通道2）
	n=TIMER_COUNT_NOW/240;                       // 更新 n（每週期點數）

	while(TIMER_COUNT_NOW <9700)                 // 掃到某上界（對應頻率範圍）
	{
			// 收集 600 點
			for(cnt1=0;cnt1<600;)
			{
				if(flag==1)
				{
					datatest[0][cnt1]=adc_ordinary_conversion_data_get(ADC2); // V
					datatest[1][cnt1]=adc_ordinary_conversion_data_get(ADC1); // I1
					datatest[2][cnt1]=adc_ordinary_conversion_data_get(ADC3); // I2
					cnt1++;
					flag=0;
				}
			}

			// 收滿開始計算
			if(cnt1==600)
			{
				TMR8_Stop();                     // 停 Timer8
				power_calculate();               // 通道1 功率
				power_calculate_2();             // 通道2 功率
				y_cnt=0;                          // 重設 index

				power_sum=power_sum/m;           // 平均功率（通道1）
				power_sum2=power_sum2/m;         // 平均功率（通道2）

				power_avg=power_sum*scale;       // 換算實際（通道1）
				power_avg2=power_sum2*scale2;    // 換算實際（通道2）

				// 若通道1功率更大，就更新最大點
				if ((Current_power<power_avg)||(Current_power2<power_avg2))
				{
					Current_power=power_avg;
					Current_power2=power_avg2;
					CURRENT_max_COUNT=TIMER_COUNT_NOW; // 記錄當下頻率點
				}

				power_sum=0;                      // 清
				power_sum2=0;                     // 清
				cnt1=0;                           // 重置收集索引

				delay_us(10);                     // 小延遲
				TIMER_COUNT_NOW = TIMER_COUNT_NOW + 5; // 下一個掃頻點（步進 5）
				n=TIMER_COUNT_NOW/240;            // 更新 n
				PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT); // 設新 PWM
				delay_ms(10);                     // 等待穩定

				flag=0;                           // 清旗標
				TMR8_Start();                     // 開 Timer8 讓 flag 繼續
			}
	}
}

// ===========================
// sweep2：細掃（±100 範圍，步進 1）
// ===========================
void sweep2(void)
{
	Current_power=0;                             // 清最大功率
	Current_power2=0;                            // 清最大功率2

	TIMER_COUNT_sweep2_range=TIMER_COUNT_NOW+100;// 上界 = 目前 +100
	TIMER_COUNT_NOW=TIMER_COUNT_NOW-100;         // 下界 = 目前 -100

	n=TIMER_COUNT_NOW/240;                       // 更新 n

	while(TIMER_COUNT_NOW <TIMER_COUNT_sweep2_range)
	{
		// 收 600 點
		for( cnt1=0;cnt1<600;)
		{
			if(flag==1)
			{
				datatest[0][cnt1]=adc_ordinary_conversion_data_get(ADC2); // V
				datatest[1][cnt1]=adc_ordinary_conversion_data_get(ADC1); // I1
				datatest[2][cnt1]=adc_ordinary_conversion_data_get(ADC3); // I2
				cnt1++;
				flag=0;
			}
		}

		// 收滿開始算
		if(cnt1==600)
		{
			TMR8_Stop();
			power_calculate();
			power_calculate_2();
			y_cnt=0;

			power_sum=power_sum/m;
			power_sum2=power_sum2/m;

			power_avg=power_sum*scale;
			power_avg2=power_sum2*scale2;          // 注意：這裡用 scale（原碼如此，照註解不改）

			// 找最大點（以通道1為準）
			if ((Current_power<power_avg)||(Current_power2<power_avg2))
			{
				Current_power=power_avg;
				Current_power2=power_avg2;
				CURRENT_max_COUNT=TIMER_COUNT_NOW;
			}

			power_sum=0;
			power_sum2=0;
			cnt1=0;

			delay_us(10);
			TIMER_COUNT_NOW = TIMER_COUNT_NOW + 1; // 步進 1
			n=TIMER_COUNT_NOW/240;
			PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
			delay_ms(5);

			flag=0;
			TMR8_Start();
		}
	}
}

// ===========================
// track_sweep：掉功率時的小範圍追蹤掃頻
// ===========================
void track_sweep(void)
{
	Current_power=0;                              // 清最大功率
	TIMER_COUNT_OLD=TIMER_COUNT_NOW+cnt;          // 上界
	TIMER_COUNT_NOW=TIMER_COUNT_NOW-cnt;          // 下界

	while(TIMER_COUNT_NOW <TIMER_COUNT_OLD)
	{
		// 收集 600 點（此處只讀兩路 ADC1/ADC2，第三路未讀）
		for(cnt1=0;cnt1<600;)
		{
			if(flag==1)
			{
				datatest[0][cnt1]=adc_ordinary_conversion_data_get(ADC1); // V（此處變 ADC1，原碼如此）
				datatest[1][cnt1]=adc_ordinary_conversion_data_get(ADC2); // I1（此處變 ADC2）
				cnt1++;
				flag=0;
			}
		}

		// 收滿計算
		if(cnt1==600)
		{
			TMR8_Stop();
			power_calculate();
			power_calculate_2();
			y_cnt=0;

			power_sum=power_sum/m;
			power_avg=power_sum*scale;

			// 這段 for 只是取 Vout[i] 到 max_value_now，但沒有真正取最大（原碼如此）
			for (i =50; i < 850; i++)
			{
				max_value_now=Vout[i];

				// 以 power_avg 更新最大點（放在迴圈內，但結果等同檢查多次）
				if ((Current_power<power_avg)||(Current_power2<power_avg2))
				{
					Current_power=power_avg;
					Current_power2=power_avg2;
					CURRENT_max_COUNT=TIMER_COUNT_NOW;
				}
			}

			power_sum=0;
			cnt1=0;
			flag=0;

			delay_us(10);
			TIMER_COUNT_NOW = TIMER_COUNT_NOW + 1;
			n=TIMER_COUNT_NOW/240;
			PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
			delay_ms(50);
			TMR8_Start();
		}
	}
}

// ===========================
// power_calculate：用 Vout 與 Iout 算平均功率（通道1）
// ===========================
void power_calculate(void)
{
	if(n == 0) n = 1;                             // 【防呆保護】避免 n=0 導致後續除以零 HardFault

	powertotle=0;                                 // 清單段功率累加
	power_sum=0;                                  // 清總功率累加

	filtfilt(*datatest,Vout,600,a_filter,b_filter,14);      // datatest[0] → Vout 做零相位濾波
	filtfilt(*(datatest+1),Iout,600,a_filter,b_filter,14);  // datatest[1] → Iout 做零相位濾波

	m=(600-200)/n;                                // 可用段數（前後各留 100，等於丟 200）
	y_cnt=100;                                    // 從第 100 點開始算（避開邊緣）

	for(cnt1=0;cnt1<m;cnt1++)                     // 每一段（每段長度 n）
	{
		V_offset=0;                                // 清偏移
		C_offset=0;                                // 清偏移

		// 先算此段的 V/I 偏移（平均值）
		for(cnt2=0;cnt2<n;cnt2++)
		{
			V_offset=V_offset+Vout[cnt2+y_cnt];   // 累加 V
			C_offset=C_offset+Iout[cnt2+y_cnt];   // 累加 I
		}
		dataoffset[0]=V_offset/n;                 // V 偏移（平均）
		dataoffset[1]=C_offset/n;                 // I 偏移（平均）

		// 再算去偏移後的瞬時功率累加
		for(cnt2=0;cnt2<n;cnt2++)
		{
			averagepower[0]=Vout[cnt2+y_cnt]-dataoffset[0]; // 去 V 偏移
			averagepower[1]=Iout[cnt2+y_cnt]-dataoffset[1]; // 去 I 偏移
			powertotle=powertotle+averagepower[0]*averagepower[1]; // 累加 V*I
		}

		power_sum=power_sum+(powertotle/n);        // 此段平均功率加入總和
		y_cnt=y_cnt+n;                             // 下一段起點
		powertotle=0;                              // 清段功率
	}
}

// ===========================
// power_calculate_2：用 Vout 與 Iout2 算平均功率（通道2）
// ===========================
void power_calculate_2(void)
{
	cnt2=0;                                       // 清索引
	cnt1=0;                                       // 清索引
	powertotle2=0;                                 // 清段功率
	V_offset2=0;                                   // 清 V 偏移
	C_offset2=0;                                   // 清 I 偏移
	power_sum2=0;                                  // 清總功率

	filtfilt(*datatest,Vout,600,a_filter,b_filter,14);       // V 濾波
	filtfilt(*(datatest+2),Iout2,600,a_filter,b_filter,14);  // I2 濾波

	m=(600-200)/n;                                 // 段數
	y_cnt=100;                                     // 起點

	for(cnt1=0;cnt1<m;cnt1++)
	{
		// 算偏移（注意：原碼使用 V_offset（不是 V_offset2），照註解不改）
		for(cnt2=0;cnt2<n;cnt2++)
		{
			V_offset=V_offset+Vout[cnt2+y_cnt];    // 累加 V
			C_offset2=C_offset2+Iout2[cnt2+y_cnt]; // 累加 I2
		}
		dataoffset[0]=V_offset/n;                  // V 偏移
		dataoffset[2]=C_offset/n;                  // I 偏移（原碼用 C_offset，照註解不改）

		// 算功率（注意：原碼 powertotle2 += averagepower[0]*averagepower[1]，照註解不改）
		for( cnt2=0;cnt2<n;cnt2++)
		{
			averagepower2[0]=Vout[cnt2+y_cnt]-dataoffset[0];  // 去 V 偏移
			averagepower2[1]=Iout2[cnt2+y_cnt]-dataoffset[2]; // 去 I 偏移
			powertotle2=powertotle2+averagepower[0]*averagepower[1]; // 原碼如此
		}

		power_sum2=power_sum2+(powertotle2/n);     // 段平均功率累加
		y_cnt=y_cnt+n;                              // 下一段
		powertotle2=0;                              // 清段功率
		V_offset2=0;                                // 清偏移
		C_offset2=0;                                // 清偏移
	}
}

// ===========================
// filter：IIR 單向濾波（Direct Form）
// ===========================
void filter(const double* x, double* y, int xlen, double* a, double* b, int nfilt, double* zi) {
    double tmp;                                  // 暫存
    int i, j;                                    // 索引
    double a0 = a[0];                             // a[0]

    // 若 a0 != 1，將 a/b 正規化
    if (fabs(a0 - 1.0) > 1e-12) {
        tmp = a0;
        for (i = 0; i < nfilt; i++)
				{
            b[i] /= tmp;
            a[i] /= tmp;
        }
    }

    memset(y, 0, xlen * sizeof(double));         // y 清零

    // 逐點輸出
    for (i = 0; i < xlen; i++) {
        for (j = 0; j < nfilt && j <= i; j++)
				{
            y[i] += b[j] * x[i - j];             // 分子累加
            if (j > 0) y[i] -= a[j] * y[i - j];  // 分母回授（j=0 不用）
        }
        if (zi && i < nfilt - 1) y[i] += zi[i];  // 加入初始狀態（若提供）
    }
}

// ===========================
// trmul：矩陣相乘（m×n 與 n×k）
// ===========================
void trmul(double *a, double *b, double *c, int m, int n, int k) {
    for (int i = 0; i < m; ++i)                   // 走列
        for (int j = 0; j < k; ++j) {             // 走行
            c[i * k + j] = 0.0;                   // 初始化
            for (int l = 0; l < n; ++l)           // 內積
                c[i * k + j] += a[i * n + l] * b[l * k + j];
        }
}

// ===========================
// rinv：矩陣反矩陣（高斯消去/選主元）
// ===========================
int rinv(double *a, int n) {
    int *is = malloc(n * sizeof(int));            // 記錄列交換
    int *js = malloc(n * sizeof(int));            // 記錄行交換
    if (!is || !js) return 0;                     // 配置失敗

    for (int k = 0; k < n; ++k) {                 // 第 k 步
        double d = 0.0;                           // 目前最大主元
        for (int i = k; i < n; ++i)
            for (int j = k; j < n; ++j) {
                double p = fabs(a[i * n + j]);    // 絕對值
                if (p > d) {                      // 找最大
                    d = p;
                    is[k] = i;                    // 記錄列
                    js[k] = j;                    // 記錄行
                }
            }

        if (d + 1.0 == 1.0) {                     // 主元太小（奇異）
            free(is); free(js);
            return 0;
        }

        // 列交換
        if (is[k] != k)
            for (int j = 0; j < n; ++j) {
                double tmp = a[k * n + j];
                a[k * n + j] = a[is[k] * n + j];
                a[is[k] * n + j] = tmp;
            }

        // 行交換
        if (js[k] != k)
            for (int i = 0; i < n; ++i) {
                double tmp = a[i * n + k];
                a[i * n + k] = a[i * n + js[k]];
                a[i * n + js[k]] = tmp;
            }

        double pivot = a[k * n + k];              // 主元
        a[k * n + k] = 1.0 / pivot;               // 主元取倒數

        // 主列縮放
        for (int j = 0; j < n; ++j)
            if (j != k) a[k * n + j] *= a[k * n + k];

        // 消去其他列
        for (int i = 0; i < n; ++i)
            if (i != k)
                for (int j = 0; j < n; ++j)
                    if (j != k)
                        a[i * n + j] -= a[i * n + k] * a[k * n + j];

        // 主行更新
        for (int i = 0; i < n; ++i)
            if (i != k) a[i * n + k] *= -a[k * n + k];
    }

    // 還原交換（倒序）
    for (int k = n - 1; k >= 0; --k) {
        if (js[k] != k)
            for (int j = 0; j < n; ++j) {
                double tmp = a[k * n + j];
                a[k * n + j] = a[js[k] * n + j];
                a[js[k] * n + j] = tmp;
            }

        if (is[k] != k)
            for (int i = 0; i < n; ++i) {
                double tmp = a[i * n + k];
                a[i * n + k] = a[i * n + is[k]];
                a[i * n + is[k]] = tmp;
            }
    }

    free(is); free(js);                            // 釋放記憶體
    return 1;                                      // 成功
}

// ===========================
// filtfilt：零相位濾波（前向+反向）
// ===========================
int filtfilt(float* x, double* y, int xlen, double* a, double* b, int nfilt) {
    {
    int nfact;                                     // 邊界延伸長度（nfilt-1）
    int tlen;                                      // 延伸後總長
    int i;                                         // 索引
    double *tx,*tx1,*p,*t,*end;                     // 暫存序列指標
    double *sp,*tvec,*zi;                           // 解初始狀態用矩陣/向量
    double tmp,tmp1;                                // 暫存
    float *px;                                      // 新增：float 專用指標

    nfact=nfilt-1;                                  // 需要的階數（邊界）
    if(xlen<=3*nfact || nfilt<2) return -1;         // 資料太短或濾波器太短就失敗

    // 建立反射延伸序列：長度 = xlen + 6*nfact
    tlen=6*nfact+xlen;
    tx=(double *)malloc(tlen*sizeof(double));       // 延伸後序列
    tx1=(double *)malloc(tlen*sizeof(double));      // 濾波暫存

    sp=(double *)malloc( sizeof(double) * nfact * nfact ); // 狀態矩陣
    tvec=(double *)malloc( sizeof(double) * nfact );        // 向量
    zi=(double *)malloc( sizeof(double) * nfact );          // 初始狀態

    // 配置失敗就清掉返回
    if( !tx || !tx1 || !sp || !tvec || !zi )
		{
       free(tx);
       free(tx1);
       free(sp);
       free(tvec);
       free(zi);
       return 1;
    }

    // 反射延伸：開頭
    tmp=(double)x[0];
    t=tx;
    for(px=x+3*nfact; px>x; --px, ++t) *t = 2.0*tmp - (double)(*px);

    // 中間放原始資料
    float *px_end = x + xlen;
    px = x;
    for(; px<px_end; ++px, ++t) *t = (double)(*px);

    // 反射延伸：結尾
    tmp=(double)x[xlen-1];
    end = tx + tlen;
    px = x + xlen - 2;
    for(; t<end; --px, ++t) *t = 2.0*tmp - (double)(*px);

    // sp 清零
    end = sp + nfact*nfact;
    p=sp;
    while(p<end) *p++ = 0.0L;

    // 建立 sp（狀態轉移矩陣形式）
    sp[0]=1.0+a[1];
    for(i=1;i<nfact;i++){
       sp[i*nfact]=a[i+1];
       sp[i*nfact+i]=1.0L;
       sp[(i-1)*nfact+i]=-1.0L;
    }

    // 建立 tvec
    for(i=0;i<nfact;i++){
       tvec[i]=b[i+1]-a[i+1]*b[0];
    }

    // 解 zi（sp^-1 * tvec）
    if(rinv(sp,nfact)){
		  trmul(sp,tvec,zi,nfact,nfact,1);
    }
    else{
        free(zi);zi=NULL;                           // 反矩陣失敗就不用 zi
    }

    free(sp);
    free(tvec);

    // 第一次：前向濾波 tx -> tx1
    tmp1=tx[0];
    if(zi)
       for( p=zi,end=zi+nfact; p<end;) *(p++) *= tmp1;
    filter(tx,tx1,tlen,a,b,nfilt,zi);

    // 反轉 tx1
    for( p=tx1,end=tx1+tlen-1; p<end; p++,end--){
       tmp = *p;
       *p = *end;
       *end = tmp;
    }

    // 第二次：再濾一次（反向）
    tmp1 = (*tx1)/tmp1;
    if(zi)
       for( p=zi,end=zi+nfact; p<end;) *(p++) *= tmp1;
    filter(tx1,tx,tlen,a,b,nfilt,zi);

    // 反轉回 y（取中間 xlen 段）
    end = y+xlen;
    p = tx+3*nfact+xlen-1;
    while(y<end){
       *y++ = *p--;
    }

    free(zi);
    free(tx);
    free(tx1);

    return 0;                                      // 成功
}
}

// ===========================
// TMR8 Start/Stop（通常用來驅動 ADC 取樣旗標）
// ===========================
void TMR8_Start(void)
{
	/* TMR8 Start */
	tmr_counter_value_set(TMR8,0);                  // 清計數
	tmr_counter_enable(TMR8, TRUE);                 // 啟動計數
  tmr_channel_enable(TMR8, TMR_SELECT_CHANNEL_1, TRUE); // 啟用 CH1
	tmr_output_enable(TMR8, TRUE);                  // 允許輸出
}

void TMR8_Stop(void)
{
	/* TMR8 Stop */
	tmr_counter_enable(TMR8, FALSE);                // 停止計數
  tmr_channel_enable(TMR8, TMR_SELECT_CHANNEL_1, FALSE); // 關閉 CH1
	tmr_output_enable(TMR8, FALSE);                 // 關閉輸出
}

// ===========================
// PWM 硬體使能腳位控制（PA10）
// ===========================
void SHUTDOWN_PWM(void)
{
	gpio_bits_write(GPIOA, GPIO_PINS_10, TRUE);     // 將 PA10 拉高（關閉/保護，依硬體定義）
}

void TURN_ON_PWM(void)
{
	gpio_bits_write(GPIOA, GPIO_PINS_10, FALSE);    // 將 PA10 拉低（開啟/解除保護，依硬體定義）
}

// ===========================
// non_system_clock_config：另一套時脈配置（可能用於特殊情境）
// ===========================
void non_system_clock_config(void)
{
  crm_reset();                                     // 重置 CRM（時鐘管理）

  crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, TRUE); // 開外部晶振 HEXT

  while(crm_hext_stable_wait() == ERROR)           // 等外部晶振穩定
  {
  }

  crm_pll_config(CRM_PLL_SOURCE_HEXT_DIV, CRM_PLL_MULT_48, CRM_PLL_OUTPUT_RANGE_GT72MHZ); // PLL 設定
  crm_hext_clock_div_set(CRM_HEXT_DIV_2);          // HEXT 分頻

  crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE); // 開 PLL
  while(crm_flag_get(CRM_PLL_STABLE_FLAG) != SET)  // 等 PLL 穩定
  {
  }

  crm_ahb_div_set(CRM_AHB_DIV_1);                  // AHB 不分頻
  crm_apb2_div_set(CRM_APB2_DIV_2);                // APB2 分頻（≤120MHz）
  crm_apb1_div_set(CRM_APB1_DIV_2);                // APB1 分頻（≤120MHz）

  crm_auto_step_mode_enable(TRUE);                 // 自動步進模式（切換時鐘更安全）

  crm_sysclk_switch(CRM_SCLK_PLL);                 // 系統時鐘切到 PLL
  while(crm_sysclk_switch_status_get() != CRM_SCLK_PLL) // 等切換完成
  {
  }

  crm_auto_step_mode_enable(FALSE);                // 關自動步進
  system_core_clock_update();                      // 更新 system_core_clock
}

// ===========================
// buffer_compare：比較兩個 buffer 是否一致
// ===========================
error_status buffer_compare(uint16_t* p_buffer1, uint16_t* p_buffer2, uint16_t buffer_length)
{
  while(buffer_length--)                           // 逐元素比較
  {
    if(*p_buffer1 != *p_buffer2)                   // 任一不同就 ERROR
    {
      return ERROR;
    }
    p_buffer1++;                                   // 下一個
    p_buffer2++;                                   // 下一個
  }
  return SUCCESS;                                  // 全部相同
}
/*
 * 2026-01-19 修改紀錄：
 * 1. 修正 power_calculate_2 與通道1變數混用的 Bug。
 * 2. 將 Offset 累加器改為 double 型態，解決低功率因數下的捨去誤差。
 * 3. 根據 HIOKI 3390 實測 (20.82W)，校正 scale2 為 0.001253。
 *    (若顯示負值表示 CT 方向相反)。
 */