/*
 * main.h
 *
 *  Created on: 5 апр. 2025 г.
 *      Author: timur
 */

#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_
#include "cJSON.h"
#include "dht.h"         // для DHT_TYPE_* и dht_read_float_data
#include "driver/gpio.h" // для GPIO_* констант и gpio_config_t
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_err.h" // для esp_err_t, ESP_OK, esp_err_to_name
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h" // для ESP_LOGI, ESP_LOGE, ESP_LOGW и TAG
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_task_wdt.h" // для работы со сторожевым таймером
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h" // для pdMS_TO_TICKS
#include "freertos/event_groups.h"
#include "freertos/task.h" // для vTaskDelay, vTaskDelete
#include "led_strip_encoder.h"
#include "lwip/ip4_addr.h" // Добавьте эту строку для IP4_ADDR
#include "mqtt_client.h"
#include "nvs_flash.h"
#include <math.h>    // для fabsf
#include <stdbool.h> // для bool, true, false
#include <stddef.h>  // для NULL
#include <string.h>

// #define ESP_WIFI_SSID "Keenetic-5500"
// #define ESP_WIFI_PASSWORD "YbL-kjL-WPd-u8b"
#define ESP_WIFI_SSID "Xiaomi_3D4C"
#define ESP_WIFI_PASSWORD "timur1972"
#define MQTT_BROKER_URL "mqtt://193.43.147.210"
#define MQTT_PORT 1883
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define IP "193.43.147.201"
#define GATEWAY "193.43.147.200"
#define NETMASK "255.255.255.0"
#define DNS "8.8.8.8"

#define DHT_GPIO 17
#define DHT_TYPE DHT_TYPE_AM2301
#define DHT_READ_INTERVAL 3000 // 3 секунды между чтениями
#define DHT_MAX_RETRIES 3 // Количество попыток чтения при ошибке
#define DHT_TEMP_MIN -40.0 // Минимальная валидная температура
#define DHT_TEMP_MAX 80.0 // Максимальная валидная температура
#define DHT_HUM_MIN 0.0 // Минимальная валидная влажность
#define DHT_HUM_MAX 100.0 // Максимальная валидная влажность

#define RMT_LED_STRIP_RESOLUTION_HZ                                            \
  10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high
           // resolution)
#define RMT_LED_STRIP_GPIO_NUM 48

#define EXAMPLE_LED_NUMBERS 1
#define EXAMPLE_CHASE_SPEED_MS 1

#define FLAG_WIFI_CONNECTED (1 << 0) // 0000 0001
#define FLAG_MQTT_CONNECTED (1 << 1) // 0000 0010
#define FLAG_SENSOR_READY (1 << 2)   // 0000 0100
#define FLAG_DATA_VALID (1 << 3)     // 0000 1000
#define FLAG_ERROR_STATE (1 << 4)    // 0001 0000
#define FLAG_BATTERY_LOW (1 << 5)    // 0010 0000

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
extern EventGroupHandle_t wifi_event_group;
extern EventGroupHandle_t mqtt_event_group;
extern EventBits_t uxMqttBits;
extern EventBits_t uxWifiBits;

extern esp_err_t nvs_flash_init();
extern void wifi_init(void);
extern esp_err_t init_dht();
extern void dht_task(void *pvParameter);
// Проверка валидности значений
extern bool is_dht_reading_valid(float humidity, float temperature);
extern void start_dht_task();
extern void start_led_task();
extern uint8_t scale(float inValue, float inMinValue, float inMaxValue,
                     float outMinValue, float outMaxValue);
extern void process_task(void *pvParameter);
extern void start_process_task();
extern esp_err_t mqtt_init(void);
extern esp_err_t mqtt_init(void);
extern void mqtt_publish_dht_data(const dht_reading_t *dht_data);
extern void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
extern esp_err_t mqtt_start(void);
extern void mqtt_stop(void);
#endif /* MAIN_MAIN_H_ */
