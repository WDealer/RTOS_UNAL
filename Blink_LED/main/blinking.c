#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED_1 2

void app_main(void)
{

    gpio_reset_pin(LED_1);

    /* Set the GPIO as an output */

    gpio_set_direction(LED_1, GPIO_MODE_OUTPUT);

    /*infinite while loop*/
    while(1) {
        /* Blink ON */
        printf("LED ON\n");
        /*GPIO High*/
        gpio_set_level(LED_1, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        /* Blink OFF */
        printf("LED OFF\n");
        /*GPIO Low*/
        gpio_set_level(LED_1, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
