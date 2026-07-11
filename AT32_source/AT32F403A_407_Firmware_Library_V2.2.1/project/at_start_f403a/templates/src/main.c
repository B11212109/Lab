#include "at32f403a_407_board.h"
#include "at32f403a_407_clock.h"
#include <stdio.h>

#define HX711_GPIO_PORT   GPIOA         

// ??? HX711
#define HX711_1_DT_PIN    GPIO_PINS_2   // DATA1
#define HX711_1_SCK_PIN   GPIO_PINS_3   // SCK1

// ??? HX711
#define HX711_2_DT_PIN    GPIO_PINS_6   // DATA2
#define HX711_2_SCK_PIN   GPIO_PINS_7   // SCK2

// ??? HX711
#define HX711_3_DT_PIN    GPIO_PINS_1   // DATA3
#define HX711_3_SCK_PIN   GPIO_PINS_0   // SCK3

// ??? HX711
#define HX711_4_DT_PIN    GPIO_PINS_5   // DATA4
#define HX711_4_SCK_PIN   GPIO_PINS_4   // SCK4

int32_t val1 = 0, val2 = 0, val3 = 0, val4 = 0;
int i = 0, j = 0;

static void timebase_init(void) {
    crm_periph_clock_enable(CRM_TMR2_PERIPH_CLOCK, TRUE);
    // 240MHz / 240 = 1MHz timer tick (1us per count)
    tmr_base_init(TMR2, 0xFFFF, 239);
    tmr_cnt_dir_set(TMR2, TMR_COUNT_UP);
    tmr_counter_enable(TMR2, TRUE);
}

static uint16_t timebase_now_us(void) {
    return (uint16_t)tmr_counter_value_get(TMR2);
}

static uint16_t timebase_elapsed_us(uint16_t start, uint16_t now) {
    return (uint16_t)(now - start);
}

void HX711_GPIO_Init(void) {
    gpio_init_type gpio_init_struct;

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE); 

    gpio_default_para_init(&gpio_init_struct);

    // SCK ?? Output (??????)
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = HX711_1_SCK_PIN | HX711_2_SCK_PIN | HX711_3_SCK_PIN | HX711_4_SCK_PIN;
    gpio_init(HX711_GPIO_PORT, &gpio_init_struct);
    
    // ??????
    gpio_bits_reset(HX711_GPIO_PORT, HX711_1_SCK_PIN | HX711_2_SCK_PIN | HX711_3_SCK_PIN | HX711_4_SCK_PIN); 

    // DT ?? Input (??????)
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = HX711_1_DT_PIN | HX711_2_DT_PIN | HX711_3_DT_PIN | HX711_4_DT_PIN;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(HX711_GPIO_PORT, &gpio_init_struct);
}

// ???????,?? pin ??
int32_t HX711_Read(uint16_t dt_pin, uint16_t sck_pin) {
    int32_t count = 0;
    uint16_t t0 = 0;
    
    gpio_bits_reset(HX711_GPIO_PORT, sck_pin);

    // ?? DT ?? (Data Ready) - ?timeout?????
    t0 = timebase_now_us();
    while(gpio_input_data_bit_read(HX711_GPIO_PORT, dt_pin)) {
        // ??timeout?2ms,4???8ms,????80Hz
        if(timebase_elapsed_us(t0, timebase_now_us()) > 2000) { 
            return 0;  // ???????? 0
        }
    }

    for(j = 0; j < 24; j++) {
        gpio_bits_set(HX711_GPIO_PORT, sck_pin);
        
        count = count << 1;

        if(gpio_input_data_bit_read(HX711_GPIO_PORT, dt_pin)) {
            count++;
        }

        gpio_bits_reset(HX711_GPIO_PORT, sck_pin);
    }

    gpio_bits_set(HX711_GPIO_PORT, sck_pin);
    gpio_bits_reset(HX711_GPIO_PORT, sck_pin);

    if(count & 0x800000) {
        count |= 0xFF000000; 
    }
    
    return count;
}

int main(void)
{
    system_clock_config();
    uart_print_init(115200); 
    delay_init();
    HX711_GPIO_Init();
    timebase_init();

    while (1)
    {
        static uint16_t last_sample = 0;
        static uint16_t last_debug = 0;
        uint16_t now = timebase_now_us();
        
        // ?1????GPIO??(debug?)
        if (timebase_elapsed_us(last_debug, now) > 1000000) {
            last_debug = now;
            printf("GPIO: DT1=%d DT2=%d DT3=%d DT4=%d\r\n",
                gpio_input_data_bit_read(HX711_GPIO_PORT, HX711_1_DT_PIN),
                gpio_input_data_bit_read(HX711_GPIO_PORT, HX711_2_DT_PIN),
                gpio_input_data_bit_read(HX711_GPIO_PORT, HX711_3_DT_PIN),
                gpio_input_data_bit_read(HX711_GPIO_PORT, HX711_4_DT_PIN));
        }

        if (timebase_elapsed_us(last_sample, now) < 12500) {
            continue;
        }
        last_sample = now;

        // ??????(??:???CH1)
        val1 = HX711_Read(HX711_1_DT_PIN, HX711_1_SCK_PIN);
        val2 = HX711_Read(HX711_2_DT_PIN, HX711_2_SCK_PIN);
        val3 = HX711_Read(HX711_3_DT_PIN, HX711_3_SCK_PIN);
        val4 = HX711_Read(HX711_4_DT_PIN, HX711_4_SCK_PIN);
        
        // Debug: ????????timeout
        // printf("CH1=%ld CH2=%ld CH3=%ld CH4=%ld\r\n", val1, val2, val3, val4);
        
        // ????: val1,val2,val3,val4
        printf("%ld,%ld,%ld,%ld\r\n", val1, val2, val3, val4);
    }
}
