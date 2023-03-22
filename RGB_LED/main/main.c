/* 
    LEDC fade pattern using interruption button task

    PRESENTED BY:
    Jhon Alejandro Sanchez Sanabria C.C: 1053873573
    Edwin Andres Fiesco Cod: 817019
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include <string.h>

#define LED_RED_CHANNEL     LEDC_CHANNEL_0
#define LED_GREEN_CHANNEL   LEDC_CHANNEL_1
#define LED_BLUE_CHANNEL    LEDC_CHANNEL_2
#define LEDC_TEST_CH_NUM    (3)

#define LED_RED_PIN 18
#define LED_GREEN_PIN 19
#define LED_BLUE_PIN 21
#define INTERRUPTION_BUTTON 26

#define UART_NUM        UART_NUM_0
#define BUF_SIZE        1024
#define LED_RED         0
#define LED_GREEN       1
#define LED_BLUE        2


int current_color = LED_RED;

void uart_task(void *arg)
{
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB
    };
    uart_param_config(UART_NUM, &uart_cfg);
    uart_set_pin(UART_NUM, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0);

    while(1)
    {
        uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
        int len = uart_read_bytes(UART_NUM, data, BUF_SIZE, 20 / portTICK_PERIOD_MS);
        if(len > 0)
        {
            if(strncmp((const char *)data, "red", 3) == 0)
            {
                current_color = LED_RED;
            }
            else if(strncmp((const char *)data, "green", 5) == 0)
            {
                current_color = LED_GREEN;
            }
            else if(strncmp((const char *)data, "blue", 4) == 0)
            {
                current_color = LED_BLUE;
            }
        }
        free(data);
    }
}



/*Ledc timer configuration*/

ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_13_BIT, 
    .freq_hz = 5000,                      
    .speed_mode = LEDC_LOW_SPEED_MODE,           
    .timer_num = LEDC_TIMER_0            
};

/*Ledc colors channels configuration*/

ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
    {
        .channel    = LED_RED_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_RED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    },
    {
        .channel    = LED_GREEN_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_GREEN_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    },
    {
        .channel    = LED_BLUE_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_BLUE_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    },
};

void rgb_button_task(void *arg)
{
    while(1)
    {
        switch(current_color)
        {
            case LED_RED:
            /*ledc_fade_func used to set red led duty cycle*/
                ledc_set_fade_time_and_start(ledc_channel[LED_RED_CHANNEL].speed_mode,ledc_channel[LED_RED_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_RED_CHANNEL].speed_mode,ledc_channel[LED_RED_CHANNEL].channel,1024,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_RED_CHANNEL].speed_mode,ledc_channel[LED_RED_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                break;

            case LED_GREEN:
            /*ledc_fade_func used to set green led duty cycle*/
                ledc_set_fade_time_and_start(ledc_channel[LED_GREEN_CHANNEL].speed_mode,ledc_channel[LED_GREEN_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_GREEN_CHANNEL].speed_mode,ledc_channel[LED_GREEN_CHANNEL].channel,1024,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_GREEN_CHANNEL].speed_mode,ledc_channel[LED_GREEN_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                break;

            case LED_BLUE:
            /*ledc_fade_func used to set blue led duty cycle*/
                ledc_set_fade_time_and_start(ledc_channel[LED_BLUE_CHANNEL].speed_mode,ledc_channel[LED_BLUE_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_BLUE_CHANNEL].speed_mode,ledc_channel[LED_BLUE_CHANNEL].channel,1024,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_BLUE_CHANNEL].speed_mode,ledc_channel[LED_BLUE_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    /*Install ledc fade function in order to used in rgb task*/
    ledc_fade_func_install(0);
    
    /*Button pin intiallization*/
    esp_rom_gpio_pad_select_gpio(INTERRUPTION_BUTTON);
    gpio_set_direction(INTERRUPTION_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(INTERRUPTION_BUTTON, GPIO_PULLUP_ONLY);


    /*Ledc init*/
    ledc_timer_config(&ledc_timer);
    for(int ch = 0; ch < LEDC_TEST_CH_NUM; ch++)
    {
        ledc_channel_config(&ledc_channel[ch]);
    }

    /*Task*/
    xTaskCreate(rgb_button_task, "rgb_button_task", 2048, NULL, 10, NULL);
    xTaskCreate(uart_task, "uart_task", 2048, NULL, 10, NULL);
}
