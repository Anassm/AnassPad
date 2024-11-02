#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "stdio.h"

// Define constants for GPIO pins and SMA settings
#define KEY1_PIN 26
#define KEY2_PIN 27
#define SMA_SIZE 4
#define WMA_SIZE 4
#define KEY_AMOUNT 2

// Configuration for each sensor
struct config
{
    char name[20];         // Name of instance
    int8_t actuation;      // Actuation point in percentages
    int8_t reset;          // Reset point in percentages
    int16_t min_value;     // Calibrated min value hall sensor can read
    int16_t max_value;     // Calibrated max value hall sensor can read
    int16_t current_value; // Current value
    bool isPressed;        // Check if key is pressed
    char trigger;          // HID key to trigger
};
struct config keyConfigs[KEY_AMOUNT];

// Function declarations
void init_gpio(void);
void init_keys_config(void);
uint16_t read_adc_key(uint input_channel);
uint16_t SMA_filter(uint16_t unfiltered_value, uint16_t *buffer, uint32_t *current_sum, uint8_t *index);
uint16_t WMA_filter(uint16_t unfiltered_value, uint16_t *buffer, uint8_t *index);
uint8_t mapToPercentage(struct config *keyConfig);

// SMA data for each key, prefilled with initial ADC readings
uint16_t sma_buffer_key1[SMA_SIZE];
uint16_t sma_buffer_key2[SMA_SIZE];
uint32_t sma_sum_key1 = 0;
uint32_t sma_sum_key2 = 0;
uint8_t sma_index_key1 = 0;
uint8_t sma_index_key2 = 0;

// WMA data for each key
uint16_t wma_buffer_key1[WMA_SIZE] = {0};
uint16_t wma_buffer_key2[WMA_SIZE] = {0};
uint8_t wma_index_key1 = 0;
uint8_t wma_index_key2 = 0;
uint16_t wma_weights[WMA_SIZE] = {1, 2, 3, 4};

// Main function
int main()
{
    // Initialize standard IO and GPIO
    stdio_init_all();
    init_gpio();
    init_keys_config();

    while (true)
    {
        // Read, filter and convert values for Key 1
        uint16_t raw_key_1 = read_adc_key(1);
        keyConfigs[0].current_value = SMA_filter(raw_key_1, sma_buffer_key1, &sma_sum_key1, &sma_index_key1);
        uint8_t percentage_SMA_key_1 = mapToPercentage(&keyConfigs[0]);

        // Read, filter and convert values for Key 2
        uint16_t raw_key_2 = read_adc_key(0);
        keyConfigs[1].current_value = SMA_filter(raw_key_2, sma_buffer_key2, &sma_sum_key2, &sma_index_key2);
        uint8_t percentage_SMA_key_2 = mapToPercentage(&keyConfigs[1]);

        printf("\033[2J\033[H");
        printf("Key1: Raw: %u | SMA: %u (%u%%) | Min: %u | Max: %u\n",
               raw_key_1, keyConfigs[0].current_value, percentage_SMA_key_1,
               keyConfigs[0].min_value, keyConfigs[0].max_value);

        printf("Key2: Raw: %u | SMA: %u (%u%%) | Min: %u | Max: %u\n",
               raw_key_2, keyConfigs[1].current_value, percentage_SMA_key_2,
               keyConfigs[1].min_value, keyConfigs[1].max_value);

        // sleep_ms(250);
        sleep_ms(100);
    }

    return 0;
}

// Function definitions

// GPIO and ADC initialization function
void init_gpio(void)
{
    adc_init();
    adc_gpio_init(KEY1_PIN);
    adc_gpio_init(KEY2_PIN);
}

void init_keys_config(void)
{
    keyConfigs[0] = (struct config){"Key1", 75, 65, 2300, 2300, 2150, false, "Z"};
    keyConfigs[1] = (struct config){"Key2", 75, 65, 2300, 2300, 2090, false, "X"};

    // If both keys are being hold, the max value will be calibrated for once.
    printf("Performing initial calibration...");

    uint16_t initial_reading_key1 = read_adc_key(0);
    uint16_t initial_reading_key2 = read_adc_key(1);

    for (int i = 0; i < SMA_SIZE; i++)
    {
        sma_buffer_key1[i] = initial_reading_key1;
        sma_sum_key1 += initial_reading_key1;

        sma_buffer_key2[i] = initial_reading_key2;
        sma_sum_key2 += initial_reading_key2;
    }

    for (int i = 0; i < KEY_AMOUNT; i++)
    {
        uint16_t initial_reading = read_adc_key(i);
        if (initial_reading > keyConfigs[i].max_value)
        {
            keyConfigs[i].max_value = initial_reading;
        }
    }
}

// Read ADC value for a given input channel
uint16_t read_adc_key(uint input_channel)
{
    adc_select_input(input_channel);
    return adc_read();
}

uint16_t SMA_filter(uint16_t unfiltered_value, uint16_t *buffer, uint32_t *current_sum, uint8_t *index)
{
    *current_sum -= buffer[*index];    // Remove the oldest value from the sum
    buffer[*index] = unfiltered_value; // Add the new value to the buffer
    *current_sum += unfiltered_value;  // Update the sum

    *index = (*index + 1) % SMA_SIZE; // Update the buffer index to be circular

    return *current_sum / SMA_SIZE;
}

uint16_t WMA_filter(uint16_t unfiltered_value, uint16_t *buffer, uint8_t *index)
{
    buffer[*index] = unfiltered_value;

    uint32_t weighted_sum = 0;
    uint32_t weight_total = 0;

    for (uint8_t i = 0; i < WMA_SIZE; i++)
    {
        weighted_sum += buffer[i] * wma_weights[i];
        weight_total += wma_weights[i];
    }

    *index = (*index + 1) % WMA_SIZE; // Update the buffer index to be circular

    return (uint16_t)(weighted_sum / weight_total);
}

uint8_t mapToPercentage(struct config *keyConfig)
{
    // Update min and max based on filtered_value
    if (keyConfig->min_value > keyConfig->current_value)
    {
        keyConfig->min_value = keyConfig->current_value; // Set new minimum
    }
    else if (keyConfig->max_value < keyConfig->current_value)
    {
        keyConfig->max_value = keyConfig->current_value; // Update maximum
    }

    // Only map if max is greater than min
    if (keyConfig->max_value > keyConfig->min_value)
    {
        return (uint8_t)(((keyConfig->current_value - keyConfig->min_value) * 100) / (keyConfig->max_value - keyConfig->min_value));
    }

    return 0; // Return 0 if no valid range is available
}
