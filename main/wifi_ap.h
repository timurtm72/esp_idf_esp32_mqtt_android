#ifndef WIFI_AP_H
#define WIFI_AP_H

#include "esp_err.h"
#include "esp_http_server.h"
#include <stdbool.h>

// Предварительное объявление структуры
typedef struct {
    char ssid[33];
    char password[65];
} wifi_credentials_t;

// Настройки Access Point
#define WIFI_AP_SSID "ESP32-Config"
#define WIFI_AP_PASSWORD "12345678"  // Пароль для AP (оставь пустым для открытой сети)
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4

// Функции для работы с NVS
esp_err_t save_wifi_credentials(const wifi_credentials_t *credentials);
esp_err_t load_wifi_credentials(wifi_credentials_t *credentials);
esp_err_t clear_wifi_credentials(void);
bool has_saved_wifi_credentials(void);

// Функции веб-сервера
esp_err_t wifi_config_handler(httpd_req_t *req);
esp_err_t wifi_save_handler(httpd_req_t *req);
esp_err_t parse_post_data(char *buf, char *ssid, char *password);

// Функции сервера
esp_err_t start_webserver(void);
void stop_webserver(void);

// Функции Wi-Fi AP
esp_err_t wifi_init_ap(void);

#endif // WIFI_AP_H 