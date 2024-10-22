#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "stdio.h"

int main()
{
    stdio_init_all();

    adc_init();

    adc_gpio_init(26);
    adc_gpio_init(27);

    while (true)
    {
        // Key 1
        adc_select_input(0);
        uint16_t result_key_1 = adc_read();
        printf("ADC value key 1: %d\n", result_key_1);

        // Key 2
        adc_select_input(1);
        uint16_t result_key_2 = adc_read();
        printf("ADC value key 2: %d\n", result_key_2);
        printf("=======================================%d\n");

        sleep_ms(200);
    }
}
