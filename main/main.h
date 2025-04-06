/*
 * main.h
 *
 *  Created on: 5 апр. 2025 г.
 *      Author: timur
 */

#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_
#include "dht.h"         // для DHT_TYPE_* и dht_read_float_data
#include "driver/gpio.h" // для GPIO_* констант и gpio_config_t
#include "esp_err.h"     // для esp_err_t, ESP_OK, esp_err_to_name
#include "esp_log.h"     // для ESP_LOGI, ESP_LOGE, ESP_LOGW и TAG
#include "esp_task_wdt.h" // для работы со сторожевым таймером
#include "freertos/FreeRTOS.h" // для pdMS_TO_TICKS
#include "freertos/task.h"     // для vTaskDelay, vTaskDelete
#include <math.h>              // для fabsf
#include <stdbool.h>           // для bool, true, false
#include <stddef.h>            // для NULL
#include <string.h>
#include "driver/rmt_tx.h"

#define DHT_GPIO 17
#define DHT_TYPE DHT_TYPE_AM2301
#define DHT_READ_INTERVAL 3000 // 3 секунды между чтениями
#define DHT_MAX_RETRIES 3 // Количество попыток чтения при ошибке
#define DHT_TEMP_MIN -40.0 // Минимальная валидная температура
#define DHT_TEMP_MAX 80.0 // Максимальная валидная температура
#define DHT_HUM_MIN 0.0 // Минимальная валидная влажность
#define DHT_HUM_MAX 100.0 // Максимальная валидная влажность

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      48

#define EXAMPLE_LED_NUMBERS         1
#define EXAMPLE_CHASE_SPEED_MS      1

typedef struct {
	float red;
	float green;
	float blue;
	float brightness;	
} rgb_led_values_t;

typedef struct {
  float temperature;
  float humidity;
  bool valid;
} dht_reading_t;

extern QueueHandle_t qDhtQueue;
extern TaskHandle_t thDhtHandle;
extern QueueHandle_t qProcessQueue;
extern TaskHandle_t thProcessHandle;
extern QueueHandle_t qLedQueue;
extern TaskHandle_t thLedHandle;
extern rgb_led_values_t rgb_led_values;


extern esp_err_t init_dht();

extern void dht_task(void *pvParameter);

// Проверка валидности значений
extern bool is_dht_reading_valid(float humidity, float temperature);

extern void dht_start_task();

extern void led_start_task();

extern uint8_t scale(float inValue,float inMinValue,float inMaxValue,float outMinValue,float outMaxValue);

extern void process_task(void *pvParameter);

extern void process_start_task();




#endif /* MAIN_MAIN_H_ */
