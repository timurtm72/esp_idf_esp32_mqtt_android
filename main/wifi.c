#include "freertos/projdefs.h"
#include "main.h"

static const char *TAG = "WIFI";
esp_netif_t *sta_netif;
EventGroupHandle_t wifi_event_group;
EventBits_t uxWifiBits;
// Функция для инициализации Wi-Fi
void wifi_init(void) {
  // Создание группы событий
  wifi_event_group = xEventGroupCreate();

  ESP_LOGI(TAG, "Initializing WiFi...");
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Создаем сетевой интерфейс для Wi-Fi станции
  sta_netif = esp_netif_create_default_wifi_sta();

  // Инициализация Wi-Fi
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Регистрация обработчиков событий
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

  // Конфигурация Wi-Fi
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = ESP_WIFI_SSID, // Имя Wi-Fi сети для подключения
              .password = ESP_WIFI_PASSWORD, // Пароль для Wi-Fi сети
              .threshold.authmode =
                  WIFI_AUTH_WPA2_PSK, // Минимальный уровень защиты
              .failure_retry_cnt = 5 // Количество повторных попыток
          },
  };

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "WiFi initialization completed, connecting using DHCP");
}

extern esp_mqtt_client_handle_t
    mqtt_client; // Предполагается, что определено в mqtt.c

// Обработчик событий Wi-Fi
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
  // Должно быть (если нужно ожидание):
  if (wifi_event_group) {
    uxWifiBits = xEventGroupGetBits(wifi_event_group);
  }
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "Wi-Fi started, connecting to %s...", ESP_WIFI_SSID);
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "Connected to %s", ESP_WIFI_SSID);
      // Установка бита
      xEventGroupSetBits(wifi_event_group, FLAG_WIFI_CONNECTED);
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "Disconnected from AP, trying to reconnect...");
      // Сброс бита
      xEventGroupClearBits(wifi_event_group, FLAG_WIFI_CONNECTED);
      // Останавливаем MQTT при отключении Wi-Fi
      if (mqtt_client) {
        ESP_LOGI(TAG, "Stopping MQTT due to WiFi disconnection");
        mqtt_stop();
      }
      esp_wifi_connect();
      break;

    default:
      break;
    }
  } else if (event_base == IP_EVENT) {

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP:
      if (sta_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
          ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&ip_info.ip));
        }
      }

      // Запускаем MQTT после получения IP-адреса
      if ((uxWifiBits & FLAG_WIFI_CONNECTED) &&
          ((uxMqttBits & FLAG_MQTT_CONNECTED) == 0)) {
        ESP_LOGI(TAG, "Starting MQTT after WiFi connection");
        esp_err_t result = mqtt_init();
        if (result == ESP_OK) {
          ESP_LOGI(TAG, "MQTT started successfully");
        } else {
          ESP_LOGE(TAG, "Failed to start MQTT: %s", esp_err_to_name(result));
        }
      } else {
        ESP_LOGI(TAG, "MQTT not started");
      }
      break;
    }
  }
}