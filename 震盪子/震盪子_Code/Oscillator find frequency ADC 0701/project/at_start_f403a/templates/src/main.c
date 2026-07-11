/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
  **************************************************************************
  * Copyright notice & Disclaimer
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES.
  **************************************************************************
  */

#include "arm_math.h"
#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include "flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/** @addtogroup AT32F403A_periph_template
  * @{
  */

/* ===========================
 * 巨集定義
 * =========================== */
#define DELAY                            100
#define FAST                             1
#define SLOW                             4

/* ===========================
 * PWM / Timer / 掃頻相關變數
 * =========================== */
uint32_t TIMER_COUNT_NOW = 0;             // 目前 PWM Timer 週期計數值（對應輸出頻率）
uint32_t TIMER_COUNT_sweep2_range = 0;    // 第二次掃頻使用的頻率搜尋範圍
uint32_t CURRENT_max = 0;                 // 掃頻過程中的最大功率暫存值

uint32_t n;   // 每一個 PWM 週期內的 ADC 取樣點數
uint32_t m;   // 可用於功率計算的週期數
uint32_t i;

/* ===========================
 * UART / ADC 狀態旗標
 * =========================== */
uint16_t uart_flag = 0;
uint16_t CURRENT_NOW = 0;                 // 即時電流值（peak-to-peak）
uint16_t CURRENT_max_COUNT = 0;           // 對應最大功率的 Timer 計數值
uint16_t flag;                            // ADC 取樣完成旗標
uint16_t adc_set;

/* ===========================
 * 功率計算相關變數
 * =========================== */
float Current_power;                      // 當前掃描到的最大功率（通道 1）
float Current_power2;                     // 當前掃描到的最大功率（通道 2）

float power_sum;                          // 功率累加值
float power_sum2;
float power_avg;                          // 平均功率
float power_avg2;

float powertotle;
float powertotle2;

float dataoffset[3] = {0};                // 電壓 / 電流 DC offset
float averagepower[2] = {0};              // 去除 offset 後的瞬時功率（通道 1）
float averagepower2[2] = {0};             // 去除 offset 後的瞬時功率（通道 2）

float power_avgset[1100] = {0};            // 每 30 秒紀錄一次的平均功率（通道 1）
float power_avgset2[1100] = {0};           // 每 30 秒紀錄一次的平均功率（通道 2）

float scale  = 0.000512;                   // 功率換算比例（ADC → 實際功率）
float scale2 = 0.00064;

int TIMER_COUNT_NOW_set[1100] = {0};
int tt;

/* ===========================
 * ADC 原始資料與濾波後資料
 * =========================== */
double datatest[3][600] = {{0},{0},{0}};   // ADC 原始取樣資料（電壓 / 電流）
double Vout[600]  = {0};                   // 濾波後電壓訊號
double Iout[600]  = {0};                   // 濾波後電流訊號（通道 1）
double Iout2[600] = {0};                   // 濾波後電流訊號（通道 2)

/* ===========================
 * IIR 濾波器參數
 * =========================== */
int alen = 14;
int y_cnt = 0;

double a_filter[14] = {
  1.0, -11.353416991577037, 59.58562261352162, -191.39754550073351,
  419.79787007863723, -663.8939062277794, 778.8039192469026,
 -686.1312266652211, 453.9628071876658, -222.7704344241921,
   78.80816055210454, -19.03197402817707, 2.8121227098557227,
  -0.19199855066799718
};

int blen = 14;
double b_filter[14] = {
  4.143216763182969e-14, 5.38618179213786e-13, 3.231709075282716e-12,
  1.1849599942703293e-11, 2.962399985675823e-11, 5.332319974216481e-11,
  7.109759965621975e-11, 7.109759965621975e-11, 5.332319974216481e-11,
  2.962399985675823e-11, 1.1849599942703293e-11, 3.231709075282716e-12,
  5.38618179213786e-13, 4.143216763182969e-14
};

/* ===========================
 * 系統狀態 / 計數用變數
 * =========================== */
int max_value_now;
int windowSize = 8;
int Watt = 0;

int sec_30 = 0;
int sum = 0;

int V_offset  = 0;
int V_offset2 = 0;
int C_offset  = 0;
int C_offset2 = 0;

int V_sum = 0, V_sum2 = 0;
int i_sum = 0, i_sum2 = 0;

int TIMER_COUNT_OLD;
int count_uart = 0;
int cnttri;
int error_message;
int seccnt = 0;

int cnt1, cnt2;
int test11, test22;

int y = 0;
int ststem = 1;
int cnt;

/**
  * @brief  Print 600 points of ADC data to UART
  */
void print_adc_data(void)
{
    printf("--- ADC DATA START ---\r\n");
    printf("V I1 I2\r\n");
    for(int k = 0; k < 600; k++)
    {
        printf("%d %d %d\r\n", (uint16_t)datatest[0][k], (uint16_t)datatest[1][k], (uint16_t)datatest[2][k]);
        
        if (k % 10 == 0) delay_ms(1);
    }
    printf("--- ADC DATA END ---\r\n");
}

/* ===========================
 * 系統時脈結構
 * =========================== */
crm_clocks_freq_type crm_clocks_freq_struct = {0};

/* 函式宣告 */
void print_adc_data(void);

/**
  * @brief  main function
  * @param  none
  * @retval none
  */
int main(void)
{
    /* ===========================
     * 系統初始化
     * =========================== */
    system_clock_config();               // 系統時脈設定
    delay_init();                        // 延遲模組初始化
    crm_clocks_freq_get(&crm_clocks_freq_struct);

    uart_print_init(230400);              // UART 初始化（230400 bps）
    delay_ms(1);

    gpio_config();                       // GPIO 腳位設定
    GPIO_Init();

    TIM8_Init();                         // TMR8：ADC 取樣觸發 Timer
    ADC_Init();                          // ADC 初始化
    TIM1_Init();                         // TMR1：PWM 主輸出 Timer

    TURN_ON_PWM();                       // 啟用 PWM 驅動
    TMR1_Start();                        // 啟動 PWM Timer

    /* ===========================
     * 初始 PWM 頻率設定
     * =========================== */
    TIMER_COUNT_NOW = FREQUECY_UPPER_LIMIT;   // 設定起始掃頻頻率（高頻端）
    PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);

    n = TIMER_COUNT_NOW / 240;            // 依 PWM 頻率計算每週期取樣點數
    delay_ms(5);

    TMR8_Start();                         // 啟動 ADC 取樣 Timer
    i = 0;

    /* ===========================
     * 第一次粗略掃頻
     * =========================== */
    sweep1();                             // 粗略掃描最大功率頻率
    printf("sweep1\r\n");

    TIMER_COUNT_NOW = CURRENT_max_COUNT;  // 設定到最大功率對應頻率
    printf("%4d %3.2f %3.2f\n",
           TIMER_COUNT_NOW,
           Current_power,
           Current_power2);

    /* ===========================
     * 第二次精細掃頻
     * =========================== */
    sweep2();                             // 精細搜尋最佳頻率
    printf("sweep2\r\n");

    TMR8_Stop();

    TIMER_COUNT_NOW = CURRENT_max_COUNT;
    printf("%4d %3.2f %3.2f\n",
           TIMER_COUNT_NOW,
           Current_power,
           Current_power2);

    PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
    n = TIMER_COUNT_NOW / 240;

    TMR8_Start();

    /* ===========================
     * 主迴圈：即時功率監測與追蹤
     * =========================== */
    while(1)
    {
        /* 約每秒進行一次功率計算 */
        if(seccnt > 50000000)
        {
            /* ===========================
             * ADC 取樣（600 點）
             * =========================== */
            for(cnt1 = 0; cnt1 < 600;)
            {
                if(flag == 1)
                {
                    datatest[0][cnt1] = adc_ordinary_conversion_data_get(ADC2); // 電壓
                    datatest[1][cnt1] = adc_ordinary_conversion_data_get(ADC1); // 電流 1
                    datatest[2][cnt1] = adc_ordinary_conversion_data_get(ADC3); // 電流 2
                    cnt1++;
                    flag = 0;
                }
            }

            if((cnt1 == 600) && (ststem == 1))
            {
                TMR8_Stop();
                print_adc_data();

                n = TIMER_COUNT_NOW / 240;    // 重新計算取樣點數

                power_calculate();            // 計算通道 1 功率
                power_calculate_2();          // 計算通道 2 功率

                y_cnt = 0;

                power_sum  = power_sum  / m;
                power_avg  = power_sum  * scale;

                power_sum2 = power_sum2 / m;
                power_avg2 = power_sum2 * scale2;

                printf("%3.2f %3.2f\n", power_avg, power_avg2);

                power_sum  = 0;
                power_sum2 = 0;

                /* ===========================
                 * 功率異常檢查
                 * =========================== */
                if((power_avg < Watt) || (power_avg2 < Watt))
                {
                    printf("%3.2f %3.2f\n",
                           power_avgset[i],
                           power_avgset2[i]);

                    error_message++;
                    TMR8_Start();

                    cnt = error_message * 10;
                    track_sweep();             // 進行追蹤掃頻

                    TIMER_COUNT_NOW = CURRENT_max_COUNT;
                    printf("%4d %3.2f %3.2f\n",
                           TIMER_COUNT_NOW,
                           Current_power,
                           Current_power2);

                    PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
										printf("%4d\n", error_message);
                }
                else
                {
                    error_message = 0;
                }

                /* ===========================
                 * 多次錯誤處理
                 * =========================== */
                if(error_message == 6)
                {
                    printf("again sweep start:\r\n");
                    sweep2();                  // 重新精細掃頻
                    TIMER_COUNT_NOW = CURRENT_max_COUNT;
                    PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
                    printf("%4d\n", TIMER_COUNT_NOW);
                    printf("again sweep done:\r\n");
                }

                if(error_message == 7)
                {
                    TMR1_Stop();               // 關閉 PWM
                    printf("system error:\r\n");
                    ststem = 0;
                }

                /* ===========================
                 * 每 30 秒紀錄一次資料
                 * =========================== */
                power_sum = 0;
                sec_30++;

                if(sec_30 == 1)
                {
                    TIMER_COUNT_NOW_set[count_uart] = TIMER_COUNT_NOW;
                    power_avgset[count_uart]  = power_avg;
                    power_avgset2[count_uart] = power_avg2;
                    count_uart++;
                    sec_30 = 0;
                }

                TMR8_Start();
            }

            seccnt = 0;

            /* ===========================
             * UART 資料輸出
             * =========================== */
            if(uart_flag == 1)
            {
                printf("power calculate:\r\n");
                for(i = 0; i < (count_uart + 1); i++)
                {
                    printf("%3.2f %3.2f %4d\n",
                           power_avgset[i],
                           power_avgset2[i],
                           TIMER_COUNT_NOW_set[i]);
                }
                uart_flag = 0;
            }
        }
    }
}
/**
  * @brief  第一次粗略掃頻
  *         由高頻往低頻掃描，找出最大輸出功率的頻率點
  */
void sweep1(void)
{
    Current_power  = 0;
    Current_power2 = 0;

    n = TIMER_COUNT_NOW / 240;

    /* 掃頻範圍：約 28.2kHz → 26kHz */
    while(TIMER_COUNT_NOW < 9700)
    {
        /* ADC 取樣 600 點 */
        for(cnt1 = 0; cnt1 < 600;)
        {
            if(flag == 1)
            {
                datatest[0][cnt1] = adc_ordinary_conversion_data_get(ADC2); // 電壓
                datatest[1][cnt1] = adc_ordinary_conversion_data_get(ADC1); // 電流 1
                datatest[2][cnt1] = adc_ordinary_conversion_data_get(ADC3); // 電流 2
                cnt1++;
                flag = 0;
            }
        }

        if(cnt1 == 600)
        {
            TMR8_Stop();

            power_calculate();              // 通道 1 功率
            power_calculate_2();            // 通道 2 功率
            y_cnt = 0;

            power_sum  = power_sum  / m;
            power_sum2 = power_sum2 / m;

            power_avg  = power_sum  * scale;
            power_avg2 = power_sum2 * scale2;

            /* 更新最大功率對應頻率 */
            if((Current_power < power_avg))
            {
                Current_power  = power_avg;
                Current_power2 = power_avg2;
                CURRENT_max_COUNT = TIMER_COUNT_NOW;
            }

            power_sum  = 0;
            power_sum2 = 0;
            cnt1 = 0;

            delay_us(10);

            TIMER_COUNT_NOW += 5;            // 粗掃頻步進
            n = TIMER_COUNT_NOW / 240;

            PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
            delay_ms(10);

            TMR8_Start();
            flag = 0;
        }
    }
}

/**
  * @brief  第二次精細掃頻
  *         於第一次掃頻找到的最大功率附近進行細部搜尋
  */
void sweep2(void)
{
    Current_power  = 0;
    Current_power2 = 0;

    TIMER_COUNT_sweep2_range = TIMER_COUNT_NOW + 100;
    TIMER_COUNT_NOW          = TIMER_COUNT_NOW - 100;
    n = TIMER_COUNT_NOW / 240;

    while(TIMER_COUNT_NOW < TIMER_COUNT_sweep2_range)
    {
        for(cnt1 = 0; cnt1 < 600;)
        {
            if(flag == 1)
            {
                datatest[0][cnt1] = adc_ordinary_conversion_data_get(ADC2); // 電壓
                datatest[1][cnt1] = adc_ordinary_conversion_data_get(ADC1); // 電流 1
                datatest[2][cnt1] = adc_ordinary_conversion_data_get(ADC3); // 電流 2
                cnt1++;
                flag = 0;
            }
        }

        if(cnt1 == 600)
        {
            TMR8_Stop();

            power_calculate();
            power_calculate_2();
            y_cnt = 0;

            power_sum  = power_sum  / m;
            power_sum2 = power_sum2 / m;

							power_avg  = power_sum  * scale;
							power_avg2 = power_sum2 * scale;

							if((Current_power < power_avg))
							{
									Current_power  = power_avg;
                Current_power2 = power_avg2;
                CURRENT_max_COUNT = TIMER_COUNT_NOW;
            }

            power_sum  = 0;
            power_sum2 = 0;
            cnt1 = 0;

            delay_us(10);

            TIMER_COUNT_NOW += 1;             // 細掃頻步進
            n = TIMER_COUNT_NOW / 240;

            PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
            delay_ms(5);

            TMR8_Start();
            flag = 0;
        }
    }

    /* 預留安全邊界，避免落在邊緣點 */
    CURRENT_max_COUNT = CURRENT_max_COUNT - 10;
}

/**
  * @brief  追蹤掃頻
  *         當輸出功率下降時，在附近頻率動態重新搜尋最佳點
  */
void track_sweep(void)
{
    Current_power = 0;

    TIMER_COUNT_OLD = TIMER_COUNT_NOW + cnt;
    TIMER_COUNT_NOW = TIMER_COUNT_NOW - cnt;

    while(TIMER_COUNT_NOW < TIMER_COUNT_OLD)
    {
        for(cnt1 = 0; cnt1 < 600;)
        {
            if(flag == 1)
            {
                datatest[0][cnt1] = adc_ordinary_conversion_data_get(ADC1); // 電壓
                datatest[1][cnt1] = adc_ordinary_conversion_data_get(ADC2); // 電流
                cnt1++;
                flag = 0;
            }
        }

        if(cnt1 == 600)
        {
            TMR8_Stop();

            power_calculate();
            power_calculate_2();
            y_cnt = 0;

            power_sum = power_sum / m;
            power_avg = power_sum * scale;

            for(i = 50; i < 850; i++)
            {
                max_value_now = Vout[i];

                if((Current_power < power_avg))
                {
                    Current_power  = power_avg;
                    Current_power2 = power_avg2;
                    CURRENT_max_COUNT = TIMER_COUNT_NOW;
                }
            }

            power_sum = 0;
            cnt1 = 0;
            flag = 0;

            delay_us(10);

            TIMER_COUNT_NOW += 1;
            n = TIMER_COUNT_NOW / 240;

            PWM_FREQ_SET(TIMER_COUNT_NOW, DUTY_UPPER_LIMIT);
            delay_ms(50);

            TMR8_Start();
        }
    }
}
/**
  * @brief  計算通道 1 的平均功率
  *         流程：
  *         1. ADC 原始資料進行零相位濾波（filtfilt）
  *         2. 計算每週期 DC offset
  *         3. 去除 offset 後進行 V × I 積分
  */
void power_calculate(void)
{
    powertotle = 0;
    V_offset = 0;
    C_offset = 0;
    power_sum = 0;

    /* 零相位濾波 */
    //filtfilt(*datatest, Vout, 600, a_filter, b_filter, 14);
    //filtfilt(*(datatest + 1), Iout, 600, a_filter, b_filter, 14);

    /* 可用週期數（略過前後不穩定區段） */
    m = (600 - 200) / n;
    y_cnt = 100;

    for(cnt1 = 0; cnt1 < m; cnt1++)
    {
        for(cnt2 = 0; cnt2 < n; cnt2++)
        {
            V_offset += Vout[cnt2 + y_cnt];
            C_offset += Iout[cnt2 + y_cnt];
        }

        dataoffset[0] = V_offset / n;
        dataoffset[1] = C_offset / n;

        for(cnt2 = 0; cnt2 < n; cnt2++)
        {
            averagepower[0] = Vout[cnt2 + y_cnt] - dataoffset[0];
            averagepower[1] = Iout[cnt2 + y_cnt] - dataoffset[1];
            powertotle += averagepower[0] * averagepower[1];
        }

        power_sum += (powertotle / n);
        y_cnt += n;

        powertotle = 0;
        V_offset = 0;
        C_offset = 0;
    }
}

/**
  * @brief  計算通道 2 的平均功率
  */
void power_calculate_2(void)
{
    cnt2 = 0;
    cnt1 = 0;

    powertotle2 = 0;
    V_offset2 = 0;
    C_offset2 = 0;
    power_sum2 = 0;

    //filtfilt(*datatest, Vout, 600, a_filter, b_filter, 14);
    //filtfilt(*(datatest + 2), Iout2, 600, a_filter, b_filter, 14);

    m = (600 - 200) / n;
    y_cnt = 100;

    for(cnt1 = 0; cnt1 < m; cnt1++)
    {
        for(cnt2 = 0; cnt2 < n; cnt2++)
        {
            V_offset  += Vout[cnt2 + y_cnt];
            C_offset2 += Iout2[cnt2 + y_cnt];
        }

        dataoffset[0] = V_offset / n;
        dataoffset[2] = C_offset / n;

        for(cnt2 = 0; cnt2 < n; cnt2++)
        {
            averagepower2[0] = Vout[cnt2 + y_cnt] - dataoffset[0];
            averagepower2[1] = Iout2[cnt2 + y_cnt] - dataoffset[2];
            powertotle2 += averagepower[0] * averagepower[1];
        }

        power_sum2 += (powertotle2 / n);
        y_cnt += n;

        powertotle2 = 0;
        V_offset2 = 0;
        C_offset2 = 0;
    }
}

void PWM_FREQ_SET(uint32_t value, float32_t DUTY)
{
	//set TMR1 CHANNEL2 PWM frequency 
	tmr_period_value_set(TMR1, value);
	
	//ÅÜ§óduty
	tmr_channel_value_set(TMR1, TMR_SELECT_CHANNEL_2, (uint32_t)(value * DUTY) );
	tmr_counter_value_set(TMR1, 0);
}

/**
  * @brief  一維 IIR 濾波器
  * @param  x   : 輸入訊號
  * @param  y   : 輸出訊號
  * @param  xlen: 資料長度
  * @param  a,b : IIR 係數
  * @param  nfilt: 濾波器階數
  * @param  zi  : 初始狀態
  */
void filter(const double* x, double* y, int xlen,
            double* a, double* b, int nfilt, double* zi)
{
    double tmp;
    int i, j;
    double a0 = a[0];

    if(fabs(a0 - 1.0) > 1e-12)
    {
        tmp = a0;
        for(i = 0; i < nfilt; i++)
        {
            b[i] /= tmp;
            a[i] /= tmp;
        }
    }

    memset(y, 0, xlen * sizeof(double));

    for(i = 0; i < xlen; i++)
    {
        for(j = 0; j < nfilt && j <= i; j++)
        {
            y[i] += b[j] * x[i - j];
            if(j > 0)
                y[i] -= a[j] * y[i - j];
        }

        if(zi && i < nfilt - 1)
            y[i] += zi[i];
    }
}

/**
  * @brief  矩陣乘法 c = a × b
  */
void trmul(double *a, double *b, double *c, int m, int n, int k)
{
    for(int i = 0; i < m; ++i)
        for(int j = 0; j < k; ++j)
        {
            c[i * k + j] = 0.0;
            for(int l = 0; l < n; ++l)
                c[i * k + j] += a[i * n + l] * b[l * k + j];
        }
}

/**
  * @brief  矩陣反矩陣（Gauss-Jordan 法）
  * @retval 1: 成功, 0: 失敗
  */
int rinv(double *a, int n)
{
    int *is = malloc(n * sizeof(int));
    int *js = malloc(n * sizeof(int));
    if(!is || !js) return 0;

    for(int k = 0; k < n; ++k)
    {
        double d = 0.0;
        for(int i = k; i < n; ++i)
            for(int j = k; j < n; ++j)
            {
                double p = fabs(a[i * n + j]);
                if(p > d)
                {
                    d = p;
                    is[k] = i;
                    js[k] = j;
                }
            }

        if(d + 1.0 == 1.0)
        {
            free(is); free(js);
            return 0;
        }

        if(is[k] != k)
            for(int j = 0; j < n; ++j)
            {
                double tmp = a[k * n + j];
                a[k * n + j] = a[is[k] * n + j];
                a[is[k] * n + j] = tmp;
            }

        if(js[k] != k)
            for(int i = 0; i < n; ++i)
            {
                double tmp = a[i * n + k];
                a[i * n + k] = a[i * n + js[k]];
                a[i * n + js[k]] = tmp;
            }

        double pivot = a[k * n + k];
        a[k * n + k] = 1.0 / pivot;

        for(int j = 0; j < n; ++j)
            if(j != k)
                a[k * n + j] *= a[k * n + k];

        for(int i = 0; i < n; ++i)
            if(i != k)
                for(int j = 0; j < n; ++j)
                    if(j != k)
                        a[i * n + j] -= a[i * n + k] * a[k * n + j];

        for(int i = 0; i < n; ++i)
            if(i != k)
                a[i * n + k] *= -a[k * n + k];
    }

    free(is); free(js);
    return 1;
}
/**
  * @brief  零相位濾波（Forward-Backward Filtering）
  *         先正向濾波，再反向濾波以消除相位延遲
  * @retval 0: 成功, -1: 失敗
  */
int filtfilt(double* x, double* y, int xlen,
             double* a, double* b, int nfilt)
{
    int nfact;
    int tlen;
    int i;

    double *tx, *tx1;
    double *sp, *tvec, *zi;
    double *p, *t, *end;
    double tmp, tmp1;

    nfact = nfilt - 1;

    /* 資料長度不足，無法進行 filtfilt */
    if(xlen <= 3 * nfact || nfilt < 2)
        return -1;

    /* 擴展資料長度（反射法） */
    tlen = 6 * nfact + xlen;
    tx  = (double *)malloc(tlen * sizeof(double));
    tx1 = (double *)malloc(tlen * sizeof(double));

    sp   = (double *)malloc(sizeof(double) * nfact * nfact);
    tvec = (double *)malloc(sizeof(double) * nfact);
    zi   = (double *)malloc(sizeof(double) * nfact);

    if(!tx || !tx1 || !sp || !tvec || !zi)
    {
        free(tx); free(tx1);
        free(sp); free(tvec); free(zi);
        return 1;
    }

    /* 前端反射 */
    tmp = x[0];
    for(p = x + 3 * nfact, t = tx; p > x; --p, ++t)
        *t = 2.0 * tmp - *p;

    for(end = x + xlen; p < end; ++p, ++t)
        *t = *p;

    /* 後端反射 */
    tmp = x[xlen - 1];
    for(end = tx + tlen, p -= 2; t < end; --p, ++t)
        *t = 2.0 * tmp - *p;

    /* 計算初始狀態 zi */
    memset(sp, 0, sizeof(double) * nfact * nfact);
    sp[0] = 1.0 + a[1];

    for(i = 1; i < nfact; i++)
    {
        sp[i * nfact]     = a[i + 1];
        sp[i * nfact + i] = 1.0;
        sp[(i - 1) * nfact + i] = -1.0;
    }

    for(i = 0; i < nfact; i++)
        tvec[i] = b[i + 1] - a[i + 1] * b[0];

    if(rinv(sp, nfact))
        trmul(sp, tvec, zi, nfact, nfact, 1);
    else
    {
        free(zi);
        zi = NULL;
    }

    free(sp);
    free(tvec);

    /* 正向濾波 */
    tmp1 = tx[0];
    if(zi)
        for(p = zi, end = zi + nfact; p < end; p++)
            *p *= tmp1;

    filter(tx, tx1, tlen, a, b, nfilt, zi);

    /* 反轉 */
    for(p = tx1, end = tx1 + tlen - 1; p < end; p++, end--)
    {
        tmp = *p;
        *p = *end;
        *end = tmp;
    }

    /* 反向濾波 */
    tmp1 = (*tx1) / tmp1;
    if(zi)
        for(p = zi, end = zi + nfact; p < end; p++)
            *p *= tmp1;

    filter(tx1, tx, tlen, a, b, nfilt, zi);

    /* 取回有效資料 */
    end = y + xlen;
    p = tx + 3 * nfact + xlen - 1;
    while(y < end)
        *y++ = *p--;

    free(zi);
    free(tx);
    free(tx1);

    return 0;
}

/**
  * @brief  啟動 TMR1（PWM 輸出 Timer）
  */
void TMR1_Start(void)
{
    tmr_counter_value_set(TMR1, 0);
    tmr_counter_enable(TMR1, TRUE);
    tmr_channel_enable(TMR1, TMR_SELECT_CHANNEL_2, TRUE);
    tmr_output_enable(TMR1, TRUE);
}

/**
  * @brief  停止 TMR1（PWM 輸出 Timer）
  */
void TMR1_Stop(void)
{
    tmr_counter_enable(TMR1, FALSE);
    tmr_channel_enable(TMR1, TMR_SELECT_CHANNEL_2, FALSE);
    tmr_output_enable(TMR1, FALSE);
}

/**
  * @brief  啟動 TMR8（ADC 取樣 Timer）
  */
void TMR8_Start(void)
{
    tmr_counter_value_set(TMR8, 0);
    tmr_counter_enable(TMR8, TRUE);
    tmr_channel_enable(TMR8, TMR_SELECT_CHANNEL_1, TRUE);
    tmr_output_enable(TMR8, TRUE);
}

/**
  * @brief  停止 TMR8（ADC 取樣 Timer）
  */
void TMR8_Stop(void)
{
    tmr_counter_enable(TMR8, FALSE);
    tmr_channel_enable(TMR8, TMR_SELECT_CHANNEL_1, FALSE);
    tmr_output_enable(TMR8, FALSE);
}

/**
  * @brief  關閉 PWM 驅動（關斷功率輸出）
  */
void SHUTDOWN_PWM(void)
{
    gpio_bits_write(GPIOA, GPIO_PINS_10, TRUE);
}

/**
  * @brief  開啟 PWM 驅動
  */
void TURN_ON_PWM(void)
{
    gpio_bits_write(GPIOA, GPIO_PINS_10, FALSE);
}

/**
  * @brief  非系統預設的時脈設定（手動 PLL 設定）
  */
void non_system_clock_config(void)
{
    crm_reset();

    crm_clock_source_enable(CRM_CLOCK_SOURCE_HEXT, TRUE);
    while(crm_hext_stable_wait() == ERROR);

    crm_pll_config(CRM_PLL_SOURCE_HEXT_DIV,
                   CRM_PLL_MULT_48,
                   CRM_PLL_OUTPUT_RANGE_GT72MHZ);

    crm_hext_clock_div_set(CRM_HEXT_DIV_2);

    crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);
    while(crm_flag_get(CRM_PLL_STABLE_FLAG) != SET);

    crm_ahb_div_set(CRM_AHB_DIV_1);
    crm_apb2_div_set(CRM_APB2_DIV_2);
    crm_apb1_div_set(CRM_APB1_DIV_2);

    crm_auto_step_mode_enable(TRUE);
    crm_sysclk_switch(CRM_SCLK_PLL);
    while(crm_sysclk_switch_status_get() != CRM_SCLK_PLL);
    crm_auto_step_mode_enable(FALSE);

    system_core_clock_update();
}

/**
  * @brief  比較兩個 buffer 是否相同
  * @retval SUCCESS: 相同, ERROR: 不同
  */
error_status buffer_compare(uint16_t* p_buffer1,
                            uint16_t* p_buffer2,
                            uint16_t buffer_length)
{
    while(buffer_length--)
    {
        if(*p_buffer1 != *p_buffer2)
            return ERROR;
        p_buffer1++;
        p_buffer2++;
    }
    return SUCCESS;
}
