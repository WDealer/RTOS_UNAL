/* 
Sistemas en tiempo real Examen #1

Jhon Alejandro Sanchez Sanabria C.C: 1053873573
Edwin Andres Fiesco Cod: 817019

Main Features: 
* Temperatura del cautin variable por medio de UART SET_TEMP$xx$
* Histeresis variable por medio de UART SET_HIST$xx$
* Toma un promedio de 10 lecturas de temperatura por medio de un NTC
* LED 2 del ESP32 Usado para visualizar la activacion y desactivacion de la salida del  GPIO del cautin
* Por medio de dos botones es posible agregarle +5 o -5 a el rango de temperatura del cautin

Extra Features:

* el rango de RED LED puede ser cambiada por medio de UART RED$xx$xx$
* el rango de BLUE LED puede ser cambiada por medio de UART BLUE$xx$xx$
* el rango de GREEN LED puede ser cambiada por medio de UART GREEN$xx$xx$
* El LED RGB cambia de color dependiendo del nivel de temperatura del cautin

REPO LINK

https://github.com/WDealer/RTOS_UNAL

*/

/* Includes */
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "freertos/timers.h"
#include "driver/adc.h"
#include "driver/uart.h"
#include <math.h>

/* Private Define*/

#define led_red 26
#define led_green 25
#define led_blue 33

/*cautin_led_indicator is used for indicate when the cautin GPIO is enable or disable in order to visualize better the task*/

#define cautin_led_indicator 2
#define button_up 27
#define button_down 32
#define STACK_SIZE 2048
#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024
#define RESISTANCE 10000

typedef enum state_machine_
{
    INIT = 0,
    COLOR_CHECK,
    PREAMBLE_CHECK,
    MINIM_CHECK,
    MAX_CHECK,
    TEMP_CHANGE,
    SET_HIST
} state_machine_t;

state_machine_t state_machine;

/* Private Global V */


uint16_t led_blue_min = 0;
uint16_t led_blue_max = 60;

uint16_t led_green_min = 60;
uint16_t led_green_max = 70;

uint16_t led_red_min = 70;
uint16_t led_red_max = 999;

uint16_t hist = 10;
uint16_t warning_max = 70;

uint8_t led_to_change = 0;

TimerHandle_t xTimers;
TimerHandle_t xdebounceTimers;
int timerId = 1;
int debounce_timerId = 2;
uint8_t debounce_state =0;
uint8_t cautin_led_indicator_state = 0;
/* Private function define*/

void peripheral_config(void);
void task_config(void);
void isr_config(void);
void timer_config(void);

void app_main(void)
{
    peripheral_config();
    timer_config();
    task_config();
    isr_config();
}

/*-------------------------------------------------------------------------------------Main Task begins-----------------------------------------------------------------------------------------------------------*/
static void temperature_task(void *pvParameters)
{
    /* Each sec read Temperature value*/
    while (1){
        float adc_val = 0;
        float temperature = 0;
        float adc_temporal = 0;

        for(int i=0;i<=9; i++){
            adc_temporal = adc1_get_raw(ADC1_CHANNEL_6);
            adc_val+=adc_temporal;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        adc_val = adc_val/10;
        adc_val = (adc_val*3.3)/4096;
        adc_val = RESISTANCE*adc_val/(3.3-adc_val);
        temperature = 1/298.15+log(adc_val/RESISTANCE)/3977;
        temperature = 1/temperature-273.15;
                
        if ((temperature > led_red_min) & (temperature < led_red_max)){
            gpio_set_level(led_red,1);
        }else{
            gpio_set_level(led_red,0);
        }

        if ((temperature > led_green_min) & (temperature < led_green_max)){
            gpio_set_level(led_green,1);
        }else{
            gpio_set_level(led_green,0);
        }

        if ((temperature > led_blue_min) & (temperature < led_blue_max)){
            gpio_set_level(led_blue,1);
        }else{
            gpio_set_level(led_blue,0);
        }

        if (temperature >= warning_max){
            cautin_led_indicator_state = 0;
        }
        else if (temperature <= warning_max-hist){
            cautin_led_indicator_state = 1;
        }

        if ((temperature > 0) & (temperature <= warning_max)){
            if(temperature>= warning_max-hist){
                if(cautin_led_indicator_state == 1){
                    gpio_set_level(cautin_led_indicator,1);
                }else{
                    gpio_set_level(cautin_led_indicator,0);
                }
            }else{
                gpio_set_level(cautin_led_indicator,1);
            }
        }else{
            gpio_set_level(cautin_led_indicator,0);
        }

        char* Txdata = (char*) malloc(100);
        sprintf (Txdata, "The temperature is : %f\r\n", temperature);
        uart_write_bytes(UART_NUM, Txdata, strlen(Txdata));

        sprintf (Txdata, "Hysteresis range : %d  cautin max: %d\r\n", hist, warning_max);
        uart_write_bytes(UART_NUM, Txdata, strlen(Txdata));

        
        //vTaskDelay(pdMS_TO_TICKS(1000));

    }
}

static void uart_task(void *pvParameters)
{
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    /* Each sec read Temperature value*/
    while (1){
        bzero(data,BUF_SIZE);
        uint16_t temporal_min = 0;
        uint16_t temporal_max = 0;
        uint8_t index = 0;
        led_to_change =0;
        state_machine = INIT;
        index = 0;


        int len = uart_read_bytes(UART_NUM,data,BUF_SIZE,pdMS_TO_TICKS(100));

        /*State Machine where analize the different UART commands*/
        while(index <= len){
            switch (state_machine){
            /*Initial case where check the first letter before the preamble of the UART commands*/
            case INIT:
                if((len-index)>8){
                    /*Define RED uart Command in order to change the range of the RED LED*/
                    if((data[index]=='R')&&(data[index+1]=='E')&&(data[index+2]=='D')){
                        index = index + 2;
                        led_to_change = 1;
                        state_machine = PREAMBLE_CHECK;
                    }
                    /*Define BLUE UART command in order to change the range of the BLUE LED*/
                    if((data[index]=='B')&&(data[index+1]=='L')&&(data[index+2]=='U')&&(data[index+3]=='E')){
                        index = index + 3;
                        led_to_change = 2;
                        state_machine = PREAMBLE_CHECK;
                    }
                    /*Define BLUE UART command in order to change the range of the GREEN LED*/
                    if((data[index]=='G')&&(data[index+1]=='R')&&(data[index+2]=='E')&&(data[index+3]=='E')&&(data[index+4]=='N')){
                        index = index + 4;
                        led_to_change = 3;
                        state_machine = PREAMBLE_CHECK;
                    }
                    /*Define SETTEMP UART command in order to change the range of the cautin temperature*/
                    if((data[index]=='S')&&(data[index+1]=='E')&&(data[index+2]=='T')&&(data[index+3]==95)&&(data[index+4]=='T')&&(data[index+5]=='E')&&(data[index+6]=='M')&&(data[index+7]=='P')){
                        index = index + 7;
                        led_to_change = 4;
                        state_machine = PREAMBLE_CHECK;
                    }
                    /*Define SETHIST UART command in order to change the range of the Histeresis*/
                    if((data[index]=='S')&&(data[index+1]=='E')&&(data[index+2]=='T')&&(data[index+3]==95)&&(data[index+4]=='H')&&(data[index+5]=='I')&&(data[index+6]=='S')&&(data[index+7]=='T')){
                        index = index + 7;
                        led_to_change = 5;
                        state_machine = PREAMBLE_CHECK;
                    }
                }
                break;
            /*Preamble check case once Init case is successfull check if the preamble $ exits at the UART command*/
            case PREAMBLE_CHECK:
                if(data[index] == '$'){
                    if(led_to_change == 4){
                        state_machine = TEMP_CHANGE;
                    }
                    else if(led_to_change == 5){
                        state_machine = SET_HIST;
                    }
                    else{
                        state_machine = MINIM_CHECK;
                    }
                    
                }else{
                    state_machine = INIT;
                }
                break;
            /*If the preamble was successfully check, state machine checks the minimun value to change at the range of the desire UART command*/
            case MINIM_CHECK:
                if(data[index] == '$'){
                    state_machine = MAX_CHECK;
                }else{
                    if((data[index] < 48) || (data[index] > 57)){
                        state_machine = INIT;
                    }
                    else{
                        temporal_min = temporal_min*10;
                        temporal_min = temporal_min+(data[index]-48);
                    }
                }
                break;
            /*This case is the extra point of the Exam where the LEDs range could be change*/
            case MAX_CHECK:
                if(data[index] == '$'){
                    if(temporal_min <= temporal_max){
                        if(led_to_change == 1){
                            led_red_max = temporal_max;
                            led_red_min = temporal_min;
                        }
                        if(led_to_change == 2){
                            led_blue_max = temporal_max;
                            led_blue_min = temporal_min;
                        }
                        if(led_to_change == 3){
                            led_green_max = temporal_max;
                            led_green_min = temporal_min;
                        }

                        char* Txdata = (char*) malloc(100);
                        sprintf (Txdata, "led blue min : %d  led blue max: %d\r\n", led_blue_min, led_blue_max);
                        uart_write_bytes(UART_NUM, Txdata, strlen(Txdata));
                        sprintf (Txdata, "led green min : %d  led green max: %d\r\n", led_green_min, led_green_max);
                        uart_write_bytes(UART_NUM, Txdata, strlen(Txdata));
                        sprintf (Txdata, "led red min : %d  led red max: %d\r\n", led_red_min, led_red_max);
                        uart_write_bytes(UART_NUM, Txdata, strlen(Txdata));
                        
                    }
                }else{
                    if((data[index] < 48) || (data[index] > 57)){
                        state_machine = INIT;
                    }
                    else{
                        temporal_max = temporal_max*10;
                        temporal_max = temporal_max+(data[index]-48);
                    }
                }
                break;
            /*Cautin temperature range case, if TEMP UARTcommand was activated*/
            case TEMP_CHANGE:
                if(data[index] == '$'){
                    if((temporal_max > 0) & (temporal_max < 99)){
                        warning_max = temporal_max;
                        led_blue_max = warning_max-hist;
                        led_green_min = warning_max-hist;
                        led_green_max = warning_max;
                        led_red_min = warning_max;
                        led_red_max = 999;

                        char* Txdata = (char*) malloc(100);
                        sprintf (Txdata, "The cautin temperature is : %d\r\n", warning_max);
                        uart_write_bytes(UART_NUM, Txdata, strlen(Txdata));
                    }
                    state_machine=INIT;
                }
                else{
                    if((data[index] < 48) || (data[index] > 57)){
                        state_machine = INIT;
                    }
                    else{
                        temporal_max = temporal_max*10;
                        temporal_max = temporal_max+(data[index]-48);
                    }
                }

            break;
            /*Histeresis case range case, if Hist UART command was activated*/
            case SET_HIST:
                if(data[index] == '$'){
                    hist = temporal_min;
                    led_blue_max = warning_max-hist;
                    led_green_min = warning_max-hist;
                    led_green_max = warning_max;
                    led_red_min = warning_max;
                    led_red_max = 999;
                    
                    char* Txdata = (char*) malloc(100);
                    sprintf (Txdata, "The range of hysteresis is : %d\r\n", hist);
                    uart_write_bytes(UART_NUM, Txdata, strlen(Txdata));
                    state_machine=INIT;
                }
                else{
                    if((data[index] < 48) || (data[index] > 57)){
                        state_machine = INIT;
                    }
                    else{
                        temporal_min = temporal_min*10;
                        temporal_min = temporal_min+(data[index]-48);
                    }
                }

            break;

            default:
                break;
            
            }
            index++;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*-------------------------------------------------------------------------------------Main Task Ends---------------------------------------------------------------------------------------------------------------*/

/*Button handler where cautin temp can be change, it adds +5 to the temp range*/
void button1_handler(void *args)
{
    if(debounce_state){
        return;
    }
    debounce_state =1;

    if(warning_max > 0 && warning_max < 100){
        warning_max += 5;
        led_blue_max = warning_max-hist;
        led_green_min = warning_max-hist;
        led_green_max = warning_max;
        led_red_min = warning_max;
        led_red_max = 999;
    }

}
/*Button handler where cautin temp can be change, it subtract 5 from the cautin temperature range*/
void button2_handler(void *args)
{
    if(debounce_state){
        return;
    }
    debounce_state =1;

    if(warning_max > 0 && warning_max < 100){
        warning_max -= 5;
        led_blue_max = warning_max-hist;
        led_green_min = warning_max-hist;
        led_green_max = warning_max;
        led_red_min = warning_max;
        led_red_max = 999;
    }
}


/*-----------------------------------------------------------------------------------------Main peripheral configuration Begin---------------------------------------------------------------------------------------*/
void peripheral_config(void)
{

    gpio_reset_pin(led_red);
    gpio_set_direction(led_red, GPIO_MODE_OUTPUT);

    gpio_reset_pin(led_green);
    gpio_set_direction(led_green, GPIO_MODE_OUTPUT);

    gpio_reset_pin(led_blue);
    gpio_set_direction(led_blue, GPIO_MODE_OUTPUT);

    gpio_reset_pin(cautin_led_indicator);
    gpio_set_direction(cautin_led_indicator, GPIO_MODE_OUTPUT);

    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
    adc1_config_width(ADC_WIDTH_BIT_12);

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM,1,3,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM,BUF_SIZE,BUF_SIZE,0,NULL,0);
}

/*Task configuration function in ordert to maintain main app clean*/
void task_config(void)
{
    static uint8_t ucParameterToPass;
    TaskHandle_t xHandle = NULL;
    xTaskCreate(temperature_task,
                "temperature_task",
                STACK_SIZE,
                &ucParameterToPass,
                1,
                &xHandle);
    
        xTaskCreate(uart_task,
                "uart_task",
                STACK_SIZE,
                &ucParameterToPass,
                1,
                &xHandle);
}

/*Interruption Handler*/
void isr_config(void)
{
    gpio_config_t pGPIOConfig1;
    pGPIOConfig1.pin_bit_mask = (1ULL << button_down);
    pGPIOConfig1.mode = GPIO_MODE_DEF_INPUT;
    pGPIOConfig1.pull_up_en = GPIO_PULLUP_ENABLE;
    pGPIOConfig1.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pGPIOConfig1.intr_type = GPIO_INTR_NEGEDGE;

    gpio_config_t pGPIOConfig2;
    pGPIOConfig2.pin_bit_mask = (1ULL << button_up);
    pGPIOConfig2.mode = GPIO_MODE_DEF_INPUT;
    pGPIOConfig2.pull_up_en = GPIO_PULLUP_ENABLE;
    pGPIOConfig2.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pGPIOConfig2.intr_type = GPIO_INTR_NEGEDGE;


    gpio_config(&pGPIOConfig1);
    gpio_config(&pGPIOConfig2);

    
    gpio_install_isr_service(0);
    gpio_isr_handler_add(button_up , button2_handler , NULL);
    // gpio_install_isr_service(0);
    gpio_isr_handler_add(button_down , button1_handler , NULL);

}

/* Interruption stack*/
static void debounce_timer_cb(TimerHandle_t pxTimer)
{
    debounce_state =0;
}
void timer_config(void)
{

    xdebounceTimers = xTimerCreate("Debounce_timer",                      
                           (pdMS_TO_TICKS(200)),
                           pdTRUE,            
                           (void *)debounce_timerId,
                           debounce_timer_cb
    );
    if (xTimerStart(xdebounceTimers, 0) != pdPASS)
    {
        printf("timer fail \r\n");
    }
 
}

/*------------------------------------------------------------------------------Main peripheral configuration Ends------------------------------------------------------------------------------------------------*/

















