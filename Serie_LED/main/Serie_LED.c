#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED_1_GPIO 23
#define LED_2_GPIO 18
#define LED_3_GPIO 5
#define BUTTON_GPIO 22

void led_task(void *pvParameters) {
    int led = 0;
    gpio_set_direction(LED_1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_3_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);

    while(1) {
        if(gpio_get_level(BUTTON_GPIO)) {
            led++;
            if(led > 2) {
                led = 0;
            }
            gpio_set_level(LED_1_GPIO, (led == 0) ? 1 : 0);
            gpio_set_level(LED_2_GPIO, (led == 1) ? 1 : 0);
            gpio_set_level(LED_3_GPIO, (led == 2) ? 1 : 0);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
void app_main() {
    xTaskCreate(&led_task, "led_task", 2048, NULL, 5, NULL);
}