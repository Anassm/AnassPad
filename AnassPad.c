#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "stdio.h"

// Define constants for GPIO pins and SMA settings
#define KEY1_PIN 26
#define KEY2_PIN 27
#define SMA_SIZE 4
#define KEY_AMOUNT 2

// Configuration for each sensor
struct config
{
    int8_t id;
    char name[20];
    bool is_pressed;
    int8_t actuation;
    int8_t reset;
    int16_t min_value;
    int16_t max_value;
    int16_t value;
    char trigger;
};
struct config keyConfigs[KEY_AMOUNT];

// Function declarations
void init_gpio(void);
void init_keys_config(void);
uint16_t read_adc_key(uint8_t input_channel); // Changed to uint8_t
void calibrate_min_max(struct config *keyConfig, uint16_t current_value);
uint8_t calculate_percentage(const struct config *keyConfig, uint16_t current_value);
uint16_t SMA_filter(uint16_t unfiltered_value, uint16_t *buffer, uint32_t *current_sum, uint8_t *index);
void key_press(int8_t key_id);
void key_release(int8_t key_id);

// SMA data for each key, prefilled with initial ADC readings
uint16_t sma_buffer_key1[SMA_SIZE] = {0};
uint16_t sma_buffer_key2[SMA_SIZE] = {0};
uint32_t sma_sum_key1 = 0;
uint32_t sma_sum_key2 = 0;
uint8_t sma_index_key1 = 0;
uint8_t sma_index_key2 = 0;

// Main function
int main()
{
    // Initialize standard IO and GPIO
    stdio_init_all();
    init_gpio();
    init_keys_config();

    while (true)
    {
        // Key 1
        uint16_t value_key1 = SMA_filter(read_adc_key(1), sma_buffer_key1, &sma_sum_key1, &sma_index_key1);
        calibrate_min_max(&keyConfigs[0], value_key1);
        keyConfigs[0].value = calculate_percentage(&keyConfigs[0], value_key1);

        if (keyConfigs[0].value >= keyConfigs[0].actuation && !keyConfigs[0].is_pressed)
        {
            key_press(0);
        }
        else if (keyConfigs[0].value <= keyConfigs[0].reset && keyConfigs[0].is_pressed)
        {
            key_release(0);
        }

        // Key 2
        uint16_t value_key2 = SMA_filter(read_adc_key(0), sma_buffer_key2, &sma_sum_key2, &sma_index_key2);
        calibrate_min_max(&keyConfigs[1], value_key2);
        keyConfigs[1].value = calculate_percentage(&keyConfigs[1], value_key2);

        // Key 2
        if (keyConfigs[1].value >= keyConfigs[1].actuation && !keyConfigs[1].is_pressed)
        {
            key_press(1);
        }
        else if (keyConfigs[1].value <= keyConfigs[1].reset && keyConfigs[1].is_pressed)
        {
            key_release(1);
        }

        // Debugging
        printf("Key: %s | Trigger: %c | Raw ADC Value: %d | Percentage: %d%% | Actuation: %d | Reset: %d | State: %s\n",
               keyConfigs[0].name, keyConfigs[0].trigger, value_key1, keyConfigs[0].value,
               keyConfigs[0].actuation, keyConfigs[0].reset,
               keyConfigs[0].is_pressed ? "PRESSED" : "RELEASED");

        printf("Key: %s | Trigger: %c | Raw ADC Value: %d | Percentage: %d%% | Actuation: %d | Reset: %d | State: %s\n",
               keyConfigs[1].name, keyConfigs[1].trigger, value_key2, keyConfigs[1].value,
               keyConfigs[1].actuation, keyConfigs[1].reset,
               keyConfigs[1].is_pressed ? "PRESSED" : "RELEASED");

        sleep_ms(250);
    }

    return 0;
}

// GPIO and ADC initialization function
void init_gpio(void)
{
    adc_init();
    adc_gpio_init(KEY1_PIN);
    adc_gpio_init(KEY2_PIN);
}

// Initialize key configuration and initial calibration
void init_keys_config(void)
{
    // Initialize key configurations
    keyConfigs[0] = (struct config){1, "Key1", false, 75, 60, 2300, 2500, 2350, 'Z'};
    keyConfigs[1] = (struct config){2, "Key2", false, 75, 60, 2300, 2500, 2350, 'X'};

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
uint16_t read_adc_key(uint8_t input_channel) // Changed to uint8_t
{
    adc_select_input(input_channel);
    return adc_read();
}

// Calibrate min and max values based on current ADC reading
void calibrate_min_max(struct config *keyConfig, uint16_t current_value)
{
    if (current_value < keyConfig->min_value)
    {
        keyConfig->min_value = current_value;
    }
    else if (current_value > keyConfig->max_value)
    {
        keyConfig->max_value = current_value;
    }
}

// Calculate current ADC value as a percentage based on min and max calibration
uint8_t calculate_percentage(const struct config *keyConfig, uint16_t current_value)
{
    if (keyConfig->max_value > keyConfig->min_value)
    {
        return (uint8_t)(((current_value - keyConfig->min_value) * 100) /
                         (keyConfig->max_value - keyConfig->min_value));
    }
    return 0; // Return 0(%) if no valid range is available
}

// Simple moving average
uint16_t SMA_filter(uint16_t unfiltered_value, uint16_t *buffer, uint32_t *current_sum, uint8_t *index)
{
    *current_sum -= buffer[*index];    // Remove the oldest value from the sum
    buffer[*index] = unfiltered_value; // Add the new value to the buffer
    *current_sum += unfiltered_value;  // Update the sum

    *index = (*index + 1) % SMA_SIZE; // Update the buffer index to be circular

    return *current_sum / SMA_SIZE;
}

void key_press(int8_t key_id)
{
    for (int8_t i = 0; i < KEY_AMOUNT; i++)
    {
        if (keyConfigs[i].id == key_id)
        {
            keyConfigs[i].is_pressed = true;
            printf("%c, PRESSED\n", keyConfigs[i].trigger);
            return;
        }
    }
}

void key_release(int8_t key_id)
{
    for (int8_t i = 0; i < KEY_AMOUNT; i++)
    {
        if (keyConfigs[i].id == key_id)
        {
            keyConfigs[i].is_pressed = false;
            printf("%c, RELEASED\n", keyConfigs[i].trigger);
            return;
        }
    }
}
