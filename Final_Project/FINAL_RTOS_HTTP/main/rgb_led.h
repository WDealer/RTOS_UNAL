/*
 * rgb_led.h
 *
 *  Created on: Oct 11, 2021
 *      Author: kjagu
 */

#ifndef MAIN_RGB_LED_H_
#define MAIN_RGB_LED_H_

// RGB LED GPIOs
#define RGB_LED_RED_GPIO		21
#define RGB_LED_GREEN_GPIO		22
#define RGB_LED_BLUE_GPIO		23

// RGB LED color mix channels
#define RGB_LED_CHANNEL_NUM		3

// RGB LED configuration
typedef struct
{
	int channel;
	int gpio;
	int mode;
	int timer_index;
} ledc_info_t;
// ledc_info_t ledc_ch[RGB_LED_CHANNEL_NUM]; Move this declaration to the top of rgb_led.c to avoid linker errors

/**
 * Color to indicate WiFi application has started.
 */
void rgb_led_wifi_app_started(void);

/**
 * Initializes the RGB LED settings per channel, including
 * the GPIO for each color, mode and timer configuration.
 */
void rgb_led_pwm_init(void);

/**
 * Sets the RGB color.
 */
void rgb_led_set_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * Color to indicate HTTP server has started.
 */
void rgb_led_http_server_started(void);

/**
 * Color to indicate that the ESP32 is connected to an access point.
 */
void rgb_led_wifi_connected(void);

#endif /* MAIN_RGB_LED_H_ */
