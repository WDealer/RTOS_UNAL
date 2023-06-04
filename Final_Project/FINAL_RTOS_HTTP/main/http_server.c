/*
 * http_server.c
 *
 *  Created on: Oct 20, 2021
 *      Author: kjagu
 */

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "sys/param.h"
#include "driver/gpio.h"

#include "http_server.h"
#include "tasks_common.h"
#include "wifi_app.h"
#include "rgb_led.h"

// Tag used for ESP serial console messages
static const char TAG[] = "http_server";

// Firmware update status
static int g_fw_update_status = OTA_UPDATE_PENDING;

// HTTP server task handle
static httpd_handle_t http_server_handle = NULL;

// HTTP server monitor task handle
static TaskHandle_t task_http_server_monitor = NULL;

// Queue handle used to manipulate the main queue of events
static QueueHandle_t http_server_monitor_queue_handle;

static uint8_t s_led_state = 0;

extern float blue_min;
extern float blue_max;

extern float red_min;
extern float red_max;

extern float green_min;
extern float green_max;

extern float temperatura;
extern float voltage;
extern float current;
extern float power;

/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t fw_update_reset_args = {
		.callback = &http_server_fw_update_reset_callback,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "fw_update_reset"
};
esp_timer_handle_t fw_update_reset;

// Embedded files: JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t jquery_3_3_1_min_js_start[]	asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[]		asm("_binary_jquery_3_3_1_min_js_end");
extern const uint8_t index_html_start[]				asm("_binary_index_html_start");
extern const uint8_t index_html_end[]				asm("_binary_index_html_end");
extern const uint8_t app_css_start[]				asm("_binary_app_css_start");
extern const uint8_t app_css_end[]					asm("_binary_app_css_end");
extern const uint8_t app_js_start[]					asm("_binary_app_js_start");
extern const uint8_t app_js_end[]					asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[]			asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]				asm("_binary_favicon_ico_end");

/**
 * Checks the g_fw_update_status and creates the fw_update_reset timer if g_fw_update_status is true.
 */
static void http_server_fw_update_reset_timer(void)
{
	if (g_fw_update_status == OTA_UPDATE_SUCCESSFUL)
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW updated successful starting FW update reset timer");

		// Give the web page a chance to receive an acknowledge back and initialize the timer
		ESP_ERROR_CHECK(esp_timer_create(&fw_update_reset_args, &fw_update_reset));
		ESP_ERROR_CHECK(esp_timer_start_once(fw_update_reset, 8000000));
	}
	else
	{
		ESP_LOGI(TAG, "http_server_fw_update_reset_timer: FW update unsuccessful");
	}
}

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameter which can be passed to the task.
 */
static void http_server_monitor(void *parameter)
{
	http_server_queue_message_t msg;

	for (;;)
	{
		if (xQueueReceive(http_server_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
				case HTTP_MSG_WIFI_CONNECT_INIT:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_INIT");

					break;

				case HTTP_MSG_WIFI_CONNECT_SUCCESS:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_SUCCESS");

					break;

				case HTTP_MSG_WIFI_CONNECT_FAIL:
					ESP_LOGI(TAG, "HTTP_MSG_WIFI_CONNECT_FAIL");

					break;

				case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_SUCCESSFUL");
					g_fw_update_status = OTA_UPDATE_SUCCESSFUL;
					http_server_fw_update_reset_timer();

					break;

				case HTTP_MSG_OTA_UPDATE_FAILED:
					ESP_LOGI(TAG, "HTTP_MSG_OTA_UPDATE_FAILED");
					g_fw_update_status = OTA_UPDATE_FAILED;

					break;

				default:
					break;
			}
		}
	}
}

/**
 * Jquery get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "Jquery requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start, jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);

	return ESP_OK;
}

/**
 * Sends the index.html page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "index.html requested");

	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

	return ESP_OK;
}

/**
 * app.css get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.css requested");

	httpd_resp_set_type(req, "text/css");
	httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

	return ESP_OK;
}

/**
 * app.js get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "app.js requested");

	httpd_resp_set_type(req, "application/javascript");
	httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

	return ESP_OK;
}

/**
 * Sends the .ico (icon) file when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "favicon.ico requested");

	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

	return ESP_OK;
}

/**
 * Receives the .bin file fia the web page and handles the firmware update
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK, otherwise ESP_FAIL if timeout occurs and the update cannot be started.
 */
esp_err_t http_server_OTA_update_handler(httpd_req_t *req)
{
	esp_ota_handle_t ota_handle;

	char ota_buff[1024];
	int content_length = req->content_len;
	int content_received = 0;
	int recv_len;
	bool is_req_body_started = false;
	bool flash_successful = false;

	const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

	do
	{
		// Read the data for the request
		if ((recv_len = httpd_req_recv(req, ota_buff, MIN(content_length, sizeof(ota_buff)))) < 0)
		{
			// Check if timeout occurred
			if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
			{
				ESP_LOGI(TAG, "http_server_OTA_update_handler: Socket Timeout");
				continue; ///> Retry receiving if timeout occurred
			}
			ESP_LOGI(TAG, "http_server_OTA_update_handler: OTA other Error %d", recv_len);
			return ESP_FAIL;
		}
		printf("http_server_OTA_update_handler: OTA RX: %d of %d\r", content_received, content_length);

		// Is this the first data we are receiving
		// If so, it will have the information in the header that we need.
		if (!is_req_body_started)
		{
			is_req_body_started = true;

			// Get the location of the .bin file content (remove the web form data)
			char *body_start_p = strstr(ota_buff, "\r\n\r\n") + 4;
			int body_part_len = recv_len - (body_start_p - ota_buff);

			printf("http_server_OTA_update_handler: OTA file size: %d\r\n", content_length);

			esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if (err != ESP_OK)
			{
				printf("http_server_OTA_update_handler: Error with OTA begin, cancelling OTA\r\n");
				return ESP_FAIL;
			}
			else
			{
				//printf("http_server_OTA_update_handler: Writing to partition subtype %d at offset 0x%lx\r\n", update_partition->subtype, update_partition->address);
			}

			// Write this first part of the data
			esp_ota_write(ota_handle, body_start_p, body_part_len);
			content_received += body_part_len;
		}
		else
		{
			// Write OTA data
			esp_ota_write(ota_handle, ota_buff, recv_len);
			content_received += recv_len;
		}

	} while (recv_len > 0 && content_received < content_length);

	if (esp_ota_end(ota_handle) == ESP_OK)
	{
		// Lets update the partition
		if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
		{
			const esp_partition_t *boot_partition = esp_ota_get_boot_partition();
			//ESP_LOGI(TAG, "http_server_OTA_update_handler: Next boot partition subtype %d at offset 0x%lx", boot_partition->subtype, boot_partition->address);
			flash_successful = true;
		}
		else
		{
			ESP_LOGI(TAG, "http_server_OTA_update_handler: FLASHED ERROR!!!");
		}
	}
	else
	{
		ESP_LOGI(TAG, "http_server_OTA_update_handler: esp_ota_end ERROR!!!");
	}

	// We won't update the global variables throughout the file, so send the message about the status
	if (flash_successful) { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL); } else { http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED); }

	return ESP_OK;
}

/**
 * OTA status handler responds with the firmware update status after the OTA update is started
 * and responds with the compile time/date when the page is first requested
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
esp_err_t http_server_OTA_status_handler(httpd_req_t *req)
{
	char otaJSON[100];

	ESP_LOGI(TAG, "OTAstatus requested");

	sprintf(otaJSON, "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}", g_fw_update_status, __TIME__, __DATE__);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, otaJSON, strlen(otaJSON));

	return ESP_OK;
}

/**
 * DHT sensor readings JSON handler responds with DHT22 sensor data
 * @param req HTTP request for which the uri needs to be handled
 * @return ESP_OK
 */
static esp_err_t http_server_get_dht_sensor_readings_json_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/dhtSensor.json requested");

	char dhtSensorJSON[200];

	sprintf(dhtSensorJSON, "{\"temp\":\"%.1f\",\"volt\":\"%.1f\",\"current\":\"%.1f\",\"power\":\"%.1f\"}", temperatura, voltage, current, power);

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, dhtSensorJSON, strlen(dhtSensorJSON));

	return ESP_OK;
}

static esp_err_t http_server_toogle_led_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/toogle_led.json requested");

	s_led_state = !s_led_state;
	gpio_set_level(BLINK_GPIO, s_led_state);

	// Cerrar la conexion
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    
    

	return ESP_OK;
}



static esp_err_t http_server_rgb_values_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "/rgb_values.json requested");

	size_t len_red_val = 0, len_green_val = 0, len_blue_val = 0;
	char *red_str = NULL,  *green_str = NULL, *blue_str = NULL;


	// Get red header
	len_red_val = httpd_req_get_hdr_value_len(req, "red_val") + 1;
	if (len_red_val > 1)
	{
		red_str = malloc(len_red_val);
		if (httpd_req_get_hdr_value_str(req, "red_val", red_str, len_red_val) == ESP_OK)
		{
			ESP_LOGI(TAG, "red value received: %s", red_str);
		}
	}
	// Get green header
	len_green_val = httpd_req_get_hdr_value_len(req, "green_val") + 1;
	if (len_green_val > 1)
	{
		green_str = malloc(len_green_val);
		if (httpd_req_get_hdr_value_str(req, "green_val", green_str, len_green_val) == ESP_OK)
		{
			ESP_LOGI(TAG, "green value received: %s", green_str);
		}
	}
	// Get blue header
	len_blue_val = httpd_req_get_hdr_value_len(req, "blue_val") + 1;
	if (len_blue_val > 1)
	{
		blue_str = malloc(len_blue_val);
		if (httpd_req_get_hdr_value_str(req, "blue_val", blue_str, len_blue_val) == ESP_OK)
		{
			ESP_LOGI(TAG, "blue value received: %s", blue_str);
		}
	}
	// Cerrar la conexion
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
    
	rgb_led_set_color(atoi(red_str), atoi(green_str), atoi(blue_str));
	return ESP_OK;
}


static esp_err_t http_server_red_threshold_handler(httpd_req_t *req){

	size_t len_val_min = 0, len_val_max = 0;
	char *min_str = NULL,  *max_str = NULL;

	len_val_min = httpd_req_get_hdr_value_len(req, "min_val") + 1;
	len_val_max = httpd_req_get_hdr_value_len(req, "max_val") + 1;
	if (len_val_min > 1)
	{
		min_str = malloc(len_val_min);
		if (httpd_req_get_hdr_value_str(req, "min_val", min_str, len_val_min) == ESP_OK)
		{
			ESP_LOGI(TAG, "red min value received: %s", min_str);
		}
	}

	if (len_val_max > 1)
	{
		max_str = malloc(len_val_max);
		if (httpd_req_get_hdr_value_str(req, "max_val", max_str, len_val_max) == ESP_OK)
		{
			ESP_LOGI(TAG, "red max value received: %s", max_str);
		}
	}

	if (atof(min_str) <= atof(max_str)){
		red_min = atof(min_str);
		red_max = atof(max_str);
	}

    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);

	return ESP_OK;
}


static esp_err_t http_server_green_threshold_handler(httpd_req_t *req){

	size_t len_val_min = 0, len_val_max = 0;
	char *min_str = NULL,  *max_str = NULL;

	len_val_min = httpd_req_get_hdr_value_len(req, "min_val") + 1;
	len_val_max = httpd_req_get_hdr_value_len(req, "max_val") + 1;
	if (len_val_min > 1)
	{
		min_str = malloc(len_val_min);
		if (httpd_req_get_hdr_value_str(req, "min_val", min_str, len_val_min) == ESP_OK)
		{
			ESP_LOGI(TAG, "green min value received: %s", min_str);
		}
	}

	if (len_val_max > 1)
	{
		max_str = malloc(len_val_max);
		if (httpd_req_get_hdr_value_str(req, "max_val", max_str, len_val_max) == ESP_OK)
		{
			ESP_LOGI(TAG, "green max value received: %s", max_str);
		}
	}

	if (atof(min_str) <= atof(max_str)){
		green_min = atof(min_str);
		green_max = atof(max_str);
	}

	httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);

	return ESP_OK;
}


static esp_err_t http_server_blue_threshold_handler(httpd_req_t *req){

	size_t len_val_min = 0, len_val_max = 0;
	char *min_str = NULL,  *max_str = NULL;

	len_val_min = httpd_req_get_hdr_value_len(req, "min_val") + 1;
	len_val_max = httpd_req_get_hdr_value_len(req, "max_val") + 1;
	if (len_val_min > 1)
	{
		min_str = malloc(len_val_min);
		if (httpd_req_get_hdr_value_str(req, "min_val", min_str, len_val_min) == ESP_OK)
		{
			ESP_LOGI(TAG, "blue min value received: %s", min_str);
		}
	}

	if (len_val_max > 1)
	{
		max_str = malloc(len_val_max);
		if (httpd_req_get_hdr_value_str(req, "max_val", max_str, len_val_max) == ESP_OK)
		{
			ESP_LOGI(TAG, "blue max value received: %s", max_str);
		}
	}

	if (atof(min_str) <= atof(max_str)){
		blue_min = atof(min_str);
		blue_max = atof(max_str);
	}

	httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, NULL, 0);
	
	return ESP_OK;
}


/**
 * Sets up the default httpd server configuration.
 * @return http server instance handle if successful, NULL otherwise.
 */
static httpd_handle_t http_server_configure(void)
{
	// Generate the default configuration
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	// Create HTTP server monitor task
	xTaskCreatePinnedToCore(&http_server_monitor, "http_server_monitor", HTTP_SERVER_MONITOR_STACK_SIZE, NULL, HTTP_SERVER_MONITOR_PRIORITY, &task_http_server_monitor, HTTP_SERVER_MONITOR_CORE_ID);

	// Create the message queue
	http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));

	// The core that the HTTP server will run on
	config.core_id = HTTP_SERVER_TASK_CORE_ID;

	// Adjust the default priority to 1 less than the wifi application task
	config.task_priority = HTTP_SERVER_TASK_PRIORITY;

	// Bump up the stack size (default is 4096)
	config.stack_size = HTTP_SERVER_TASK_STACK_SIZE;

	// Increase uri handlers
	config.max_uri_handlers = 20;

	// Increase the timeout limits
	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;

	ESP_LOGI(TAG,
			"http_server_configure: Starting server on port: '%d' with task priority: '%d'",
			config.server_port,
			config.task_priority);

	// Start the httpd server
	if (httpd_start(&http_server_handle, &config) == ESP_OK)
	{
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

		// register query handler
		httpd_uri_t jquery_js = {
				.uri = "/jquery-3.3.1.min.js",
				.method = HTTP_GET,
				.handler = http_server_jquery_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &jquery_js);

		// register index.html handler
		httpd_uri_t index_html = {
				.uri = "/",
				.method = HTTP_GET,
				.handler = http_server_index_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &index_html);

		// register app.css handler
		httpd_uri_t app_css = {
				.uri = "/app.css",
				.method = HTTP_GET,
				.handler = http_server_app_css_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_css);

		// register app.js handler
		httpd_uri_t app_js = {
				.uri = "/app.js",
				.method = HTTP_GET,
				.handler = http_server_app_js_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &app_js);

		// register favicon.ico handler
		httpd_uri_t favicon_ico = {
				.uri = "/favicon.ico",
				.method = HTTP_GET,
				.handler = http_server_favicon_ico_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);

		// register OTAupdate handler
		httpd_uri_t OTA_update = {
				.uri = "/OTAupdate",
				.method = HTTP_POST,
				.handler = http_server_OTA_update_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_update);

		// register OTAstatus handler
		httpd_uri_t OTA_status = {
				.uri = "/OTAstatus",
				.method = HTTP_POST,
				.handler = http_server_OTA_status_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &OTA_status);

		// register dhtSensor.json handler
		httpd_uri_t dht_sensor_json = {
				.uri = "/dhtSensor.json",
				.method = HTTP_GET,
				.handler = http_server_get_dht_sensor_readings_json_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &dht_sensor_json);
		
		// register toogle_led handler
		httpd_uri_t toogle_led = {
				.uri = "/toogle_led.json",
				.method = HTTP_POST,
				.handler = http_server_toogle_led_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &toogle_led);
		
		// register rgb_receiver handler
		httpd_uri_t rgb_values = {
				.uri = "/rgb_vals.json",
				.method = HTTP_POST,
				.handler = http_server_rgb_values_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &rgb_values);

		// thresholds
		httpd_uri_t red_values = {
				.uri = "/red_vals.json",
				.method = HTTP_POST,
				.handler = http_server_red_threshold_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &red_values);


		httpd_uri_t green_values = {
				.uri = "/green_vals.json",
				.method = HTTP_POST,
				.handler = http_server_green_threshold_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &green_values);


		httpd_uri_t blue_values = {
				.uri = "/blue_vals.json",
				.method = HTTP_POST,
				.handler = http_server_blue_threshold_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &blue_values);


		return http_server_handle;
	}

	return NULL;
}

void http_server_start(void)
{
	if (http_server_handle == NULL)
	{
		http_server_handle = http_server_configure();
	}
}

void http_server_stop(void)
{
	if (http_server_handle)
	{
		httpd_stop(http_server_handle);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server");
		http_server_handle = NULL;
	}
	if (task_http_server_monitor)
	{
		vTaskDelete(task_http_server_monitor);
		ESP_LOGI(TAG, "http_server_stop: stopping HTTP server monitor");
		task_http_server_monitor = NULL;
	}
}

BaseType_t http_server_monitor_send_message(http_server_message_e msgID)
{
	http_server_queue_message_t msg;
	msg.msgID = msgID;
	return xQueueSend(http_server_monitor_queue_handle, &msg, portMAX_DELAY);
}

void http_server_fw_update_reset_callback(void *arg)
{
	ESP_LOGI(TAG, "http_server_fw_update_reset_callback: Timer timed-out, restarting the device");
	esp_restart();
}



















