/**
 * Application entry point.
 */

#include "nvs_flash.h"
#include "driver/gpio.h"
// #include "freertos/timers.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi_app.h"
#include "rgb_led.h"
#include "http_server.h"
#include <math.h>

#define power_red 26
#define power_green 25
#define power_blue 33
#define STACK_SIZE 2048


float blue_min = 0;
float blue_max = 5;

float green_min = 5;
float green_max = 10;

float red_min = 10;
float red_max = 15;

float temperatura = 30;
float voltage = 10;
float current = 2;
float power = 20;

static void ADC_init(void)
{
	adc1_config_width(ADC_WIDTH_12Bit);
	adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
	adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);
	adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
}

static void ADC_get_raw()
{
	int  volt_raw_adc_ch7 = 0, amp_raw_adc_ch0 = 0;


    /*Number of samples take in order to take the positive AC SEN*/

    int Samples = 16;
    while ( Samples > 1)
    {
        int volt_adc = adc1_get_raw(ADC1_CHANNEL_7);
        if( volt_adc > volt_raw_adc_ch7){
        volt_raw_adc_ch7 = volt_adc;
        }
        int amp_adc = adc1_get_raw(ADC1_CHANNEL_0);
        if( amp_adc > amp_raw_adc_ch0){
            amp_raw_adc_ch0 = amp_adc;
        }
        Samples--;
        vTaskDelay(1);
    }

    
	//printf("Sensor temp: %d\n\r", temp_raw_adc_ch6);
    float volt_real_adc_ch7 = ((((volt_raw_adc_ch7 - 2000.00)/400.00)*85.00)*2.00)/1.4142;
    float amp_real_adc_ch0 = (((amp_raw_adc_ch0/1692.00)*1.30)*2.00)/1.4142;

    float temp_raw_adc_ch6_temp = 0;
    float temp_raw_adc_ch6 = 0;


    /*Number of samples in order to make the temp data better*/

    for(int i=0;i<=9; i++){
        temp_raw_adc_ch6_temp = adc1_get_raw(ADC1_CHANNEL_6);
        temp_raw_adc_ch6 +=temp_raw_adc_ch6_temp;
        vTaskDelay(pdMS_TO_TICKS(1));
    }


    temp_raw_adc_ch6 = temp_raw_adc_ch6/10;
    temp_raw_adc_ch6 = (temp_raw_adc_ch6*3.3)/4096;
    temp_raw_adc_ch6 = (10000*temp_raw_adc_ch6)/(3.3-temp_raw_adc_ch6);

    float temperatura_temporal = 1/298.15+log(temp_raw_adc_ch6/10000)/3977;
    temperatura_temporal = 1/temperatura_temporal-273.15;


    printf("Sensor temp: %f\n\r", temperatura_temporal);

	printf("Sensor volt: %f\n\r", volt_real_adc_ch7);

	printf("Sensor amp: %f\n\r", amp_real_adc_ch0);



	temperatura = temperatura_temporal;
	voltage = volt_real_adc_ch7;
	current = amp_real_adc_ch0;
	power = voltage * current;

	printf("Power: %f\n\r", power);

}


static void temperature_task(void *pvParameters)
{
    /* Each sec read Power value*/
    while (1)
    {
		ADC_get_raw();

		if ((power>=red_min) && (power<=red_max)){
			gpio_set_level(power_red,1);
		}else{
			gpio_set_level(power_red,0);
		}
		if ((power>=green_min) && (power<=green_max)){
			gpio_set_level(power_green,1);
		}else{
			gpio_set_level(power_green,0);
		}
		if ((power>=blue_min) && (power<=blue_max)){
			gpio_set_level(power_blue,1);
		}else{
			gpio_set_level(power_blue,0);
		}

        vTaskDelay(pdMS_TO_TICKS(1000));
	}
    
}

/*

* LED initialization function in order to make void main app clear

*/
static void configure_led(void)
{
    
    gpio_reset_pin(BLINK_GPIO);

    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
	rgb_led_pwm_init();

	gpio_reset_pin(power_red);
    gpio_set_direction(power_red, GPIO_MODE_OUTPUT);

    gpio_reset_pin(power_green);
    gpio_set_direction(power_green, GPIO_MODE_OUTPUT);

    gpio_reset_pin(power_blue);
    gpio_set_direction(power_blue, GPIO_MODE_OUTPUT);
}


/*

*Task void

TODO: update task_common.h with main task

*/

void task_config(void){
    static uint8_t ucParameterToPass;
    TaskHandle_t xHandle = NULL;
    xTaskCreate(temperature_task,
                "temperature_task",
                STACK_SIZE,
                &ucParameterToPass,
                1,
                &xHandle);

}

void app_main(void)
{
    // Initialize NVS
	ADC_init();
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	configure_led();
	// Start Wifi
	wifi_app_start();
	task_config();
}

