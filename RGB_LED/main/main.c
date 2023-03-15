#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define LED_RED_CHANNEL           LEDC_CHANNEL_0
#define LED_GREEN_CHANNEL         LEDC_CHANNEL_1
#define LED_BLUE_CHANNEL          LEDC_CHANNEL_2
#define LEDC_TEST_CH_NUM       (3)

#define LED_RED_PIN 18
#define LED_GREEN_PIN 19
#define LED_BLUE_PIN 21
#define INTERRUPTION_BUTTON 26

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
    uint8_t button_status = 0;
    uint8_t color_index = 0;
    while(1)
    {
        button_status = gpio_get_level(INTERRUPTION_BUTTON);
        if(button_status == 1)
        {
            color_index++;
            if(color_index > 2)
            {
                color_index = 0;
            }
            switch(color_index)
            {
                case 0:
                /*ledc_fade_func used to set red led duty cycle*/
                ledc_set_fade_time_and_start(ledc_channel[LED_RED_CHANNEL].speed_mode,ledc_channel[LED_RED_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_RED_CHANNEL].speed_mode,ledc_channel[LED_RED_CHANNEL].channel,1024,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_RED_CHANNEL].speed_mode,ledc_channel[LED_RED_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);            
                    break;

                case 1:
                /*ledc_fade_func used to set green led duty cycle*/
                ledc_set_fade_time_and_start(ledc_channel[LED_GREEN_CHANNEL].speed_mode,ledc_channel[LED_GREEN_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_GREEN_CHANNEL].speed_mode,ledc_channel[LED_GREEN_CHANNEL].channel,1024,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_GREEN_CHANNEL].speed_mode,ledc_channel[LED_GREEN_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                    break;

                case 2:
                /*ledc_fade_func used to set blue led duty cycle*/
                ledc_set_fade_time_and_start(ledc_channel[LED_BLUE_CHANNEL].speed_mode,ledc_channel[LED_BLUE_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_BLUE_CHANNEL].speed_mode,ledc_channel[LED_BLUE_CHANNEL].channel,1024,500,LEDC_FADE_WAIT_DONE);
                ledc_set_fade_time_and_start(ledc_channel[LED_BLUE_CHANNEL].speed_mode,ledc_channel[LED_BLUE_CHANNEL].channel,0,500,LEDC_FADE_WAIT_DONE);
                    break;
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
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
}
