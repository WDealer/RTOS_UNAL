#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

//Define RGB LED pins
#define LED_R_PIN 26
#define LED_G_PIN 27
#define LED_B_PIN 14

// RGB LED init function

//Definie Interruption Pin

#define pin_INTERUPTION_1 15

// Define queue variable
QueueHandle_t inte_queue;

//button init

void int_button_init()
{
    gpio_set_direction(pin_INTERUPTION_1, GPIO_MODE_INPUT);
    gpio_pulldown_en(pin_INTERUPTION_1);
    gpio_pullup_dis(pin_INTERUPTION_1);
    gpio_set_intr_type(pin_INTERUPTION_1, GPIO_INTR_POSEDGE);

}

//rgb led init

void rgb_led_init()
{

    //set red LED GPIO
    //gpio_pad_select_gpio(LED_R_PIN);
    gpio_set_direction(LED_R_PIN,GPIO_MODE_OUTPUT);

    //set green LED GPIO
    //gpio_pad_select_gpio(LED_G_PIN);
    gpio_set_direction(LED_G_PIN,GPIO_MODE_OUTPUT);

    //set blue LED GPIO
    //gpio_pad_select_gpio(LED_B_PIN);
    gpio_set_direction(LED_B_PIN,GPIO_MODE_OUTPUT);

}

//RGB LED Color function
void rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    gpio_set_level(LED_R_PIN, r);
    gpio_set_level(LED_G_PIN, g);
    gpio_set_level(LED_B_PIN, b);
}

//Interruption rutine
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    int inte_value = (int)arg;
    xQueueSendFromISR(inte_queue, &inte_value, NULL);
}

//led_pattern_enum
typedef enum led_pattern_t{
    LED_OFF,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_TOTAL
} led_pattern_t;

//interruption Task

void buttonPushedTask(void *arg)
{    
    int inte_value;

    while(true)
    {
        if(xQueueReceive(inte_queue,&inte_value,portMAX_DELAY))
        {
            //disable interrupt
            gpio_isr_handler_remove(inte_value);

            //Wait while button is released

            do
            {
                vTaskDelay(20 / portTICK_PERIOD_MS);
            } while (gpio_get_level(inte_value)==1);

            //switch case to changue RGB color

            static led_pattern_t pattern_index = 0;
            pattern_index++;
            pattern_index %= LED_TOTAL;
            
            switch (pattern_index)
            {
                case LED_OFF:
                    rgb_led_set_color(0,0,0);
                    break;
                case LED_RED:
                    rgb_led_set_color(1,0,0);
                    break;
                case LED_GREEN:
                    rgb_led_set_color(0,1,0);
                    break;
                case LED_BLUE:
                    rgb_led_set_color(0,0,1);
                    break;
                default:
                    break;
            }
            // Re-enable the interrupt
            gpio_isr_handler_add(inte_value, gpio_isr_handler, (void *)inte_value);
            

        }
    }
}


void app_main(void)
{
    int_button_init();
    rgb_led_init();

    inte_queue = xQueueCreate(10, sizeof(int));
    xTaskCreate(buttonPushedTask, "buttonPushedTask", 2048, NULL, 1, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(pin_INTERUPTION_1, gpio_isr_handler, (void *)pin_INTERUPTION_1);


}
