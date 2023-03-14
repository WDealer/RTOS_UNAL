#include "esp_adc_cal.h"
#include <math.h>

#define V_REF   1100    // ADC reference voltage
#define ADC_ATTEN   ADC_ATTEN_DB_11   // ADC attenuation
#define THERM_R0    10000   // Thermistor reference resistance
#define THERM_T0    25      // Thermistor reference temperature (in Celsius)
#define THERM_A     0.001129148
#define THERM_B     0.000234125
#define THERM_C     0.0000000876741

// Calculate the temperature from the ADC reading
float therm_read_temp(int adc_val)
{
    float therm_r = (V_REF * (4095 - adc_val)) / adc_val;   // Thermistor resistance
    float therm_t = 1.0 / (THERM_A + THERM_B*log(therm_r/THERM_R0) + THERM_C*pow(log(therm_r/THERM_R0), 3));   // Temperature in Kelvin
    return therm_t - 273.15;    // Convert Kelvin to Celsius
}

// Initialize the ADC
void therm_adc_init()
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN);
}

// Read the ADC value
int therm_adc_read()
{
    return adc1_get_raw(ADC1_CHANNEL_0);
}

// Main function
void app_main()
{
    therm_adc_init();

    while (1) {
        int adc_val = therm_adc_read();
        float temp = therm_read_temp(adc_val);
        printf("Temperature: %.2f C\n", temp);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}