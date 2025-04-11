#include "esp_mac.h"
#include "main.h"

static const char *TAG = "mqtt";

// Глобальная переменная для MQTT клиента
esp_mqtt_client_handle_t mqtt_client = NULL;

// Топики MQTT
#define MQTT_TOPIC_DHT_DATA                                                    \
  "esp32/sensor/dht" // Отправка данных о температуре и влажности
#define MQTT_TOPIC_RGB_CONTROL                                                 \
  "esp32/control/rgb" // Получение управляющих команд для светодиода

EventBits_t uxMqttBits;
EventGroupHandle_t mqtt_event_group;
// Обработчик событий MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  if (mqtt_event_group) {
    // Ожидание конкретного бита
    uxMqttBits = xEventGroupGetBits(mqtt_event_group);
  }
  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT connected to broker");
    // Установка бита
    xEventGroupSetBits(mqtt_event_group, FLAG_MQTT_CONNECTED);

    // Подписываемся на топик управления RGB светодиодом
    esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_RGB_CONTROL, 0);
    ESP_LOGI(TAG, "Subscribed to topic: %s", MQTT_TOPIC_RGB_CONTROL);
    break;

  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT disconnected from broker");
    // Сброс бита
    xEventGroupClearBits(mqtt_event_group, FLAG_MQTT_CONNECTED);
    esp_mqtt_client_unsubscribe(mqtt_client, MQTT_TOPIC_RGB_CONTROL);
    mqtt_start();
    break;

  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TAG, "MQTT subscribed to topic");
    break;

  case MQTT_EVENT_DATA:
    if ((uxWifiBits & FLAG_WIFI_CONNECTED) &&
        (uxMqttBits & FLAG_MQTT_CONNECTED)) {
      // Обработка полученных данных
      ESP_LOGI(TAG, "MQTT data received: topic=%.*s, data=%.*s",
               event->topic_len, event->topic, event->data_len, event->data);
      // Проверяем, от какого топика пришли данные
      if (event->topic_len == strlen(MQTT_TOPIC_RGB_CONTROL) &&
          strncmp(event->topic, MQTT_TOPIC_RGB_CONTROL, event->topic_len) ==
              0) {

        // Создаем буфер и копируем данные
        char *data = malloc(event->data_len + 1);
        if (!data) {
          ESP_LOGE(TAG, "Memory allocation error");
          break;
        }

        memcpy(data, event->data, event->data_len);
        data[event->data_len] = 0; // Добавляем завершающий нуль

        // Парсим JSON
        cJSON *root = cJSON_Parse(data);
        if (root) {
          // Создаем структуру для хранения значений RGB
          rgb_led_values_t rgb_values = {0};

          // Извлекаем значения из JSON
          cJSON *red = cJSON_GetObjectItem(root, "red");
          cJSON *green = cJSON_GetObjectItem(root, "green");
          cJSON *blue = cJSON_GetObjectItem(root, "blue");
          cJSON *brightness = cJSON_GetObjectItem(root, "brightness");

          // Проверяем и устанавливаем значения
          if (red && cJSON_IsNumber(red)) {
            rgb_values.red = (float)red->valuedouble;
          }

          if (green && cJSON_IsNumber(green)) {
            rgb_values.green = (float)green->valuedouble;
          }

          if (blue && cJSON_IsNumber(blue)) {
            rgb_values.blue = (float)blue->valuedouble;
          }

          if (brightness && cJSON_IsNumber(brightness)) {
            rgb_values.brightness = (float)brightness->valuedouble;
          }

          // Отправляем данные в очередь для задачи LED
          if (xQueueSend(qLedQueue, &rgb_values, pdMS_TO_TICKS(100)) !=
              pdPASS) {
            ESP_LOGE(TAG, "Failed to send data to LED queue");
          } else {
            ESP_LOGI(TAG, "RGB data sent: R=%.1f, G=%.1f, B=%.1f, Br=%.1f",
                     rgb_values.red, rgb_values.green, rgb_values.blue,
                     rgb_values.brightness);
          }

          // Освобождаем память
          cJSON_Delete(root);
        } else {
          ESP_LOGE(TAG, "JSON parsing error: %s", data);
        }

        // Освобождаем память
        free(data);
      }
    }
    break;

  case MQTT_EVENT_ERROR:
    if (event->error_handle) {
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x",
                 event->error_handle->esp_tls_last_esp_err);
        ESP_LOGE(TAG, "Last tls stack error number: 0x%x",
                 event->error_handle->esp_tls_stack_err);
        ESP_LOGE(TAG, "Last captured errno : %d (%s)",
                 event->error_handle->esp_transport_sock_errno,
                 strerror(event->error_handle->esp_transport_sock_errno));
      }
    }
    if (mqtt_client) {
      mqtt_stop();
      ESP_LOGE(TAG, "MQTT error occurred and stoped");
    }
    break;

  default:
    break;
  }
}

// Инициализация MQTT клиента
esp_err_t mqtt_init(void) {
  // Создание группы событий
  mqtt_event_group = xEventGroupCreate();
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  char unique_client_id[32];
  sprintf(unique_client_id, "esp32_%02x%02x%02x%02x%02x%02x", mac[0], mac[1],
          mac[2], mac[3], mac[4], mac[5]);

  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = MQTT_BROKER_URL,
      .broker.address.port = MQTT_PORT,
      .credentials.username = MQTT_USERNAME,
      .credentials.authentication.password = MQTT_PASSWORD,
      .network.timeout_ms = 60000, // увеличьте таймаут до 60 секунд
      .task.stack_size = 8192, // увеличьте размер стека
      .credentials.client_id = unique_client_id,
      // В функции mqtt_init()
      .session.keepalive = 120, // Увеличьте с дефолтных 60 до 120 секунд
      // В функции mqtt_init()
      .network.reconnect_timeout_ms = 1000, // Повторная попытка через 10 секунд
      .network.disable_auto_reconnect = true,
      // В функции mqtt_init()
      .broker.verification.skip_cert_common_name_check = true,
      // В функции mqtt_init()
      .session.disable_clean_session = true,
  };

  ESP_LOGI(TAG, "Initializing MQTT client with broker URL: %s",
           MQTT_BROKER_URL);
  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  if (!mqtt_client) {
    ESP_LOGE(TAG, "MQTT client initialization error");
    return ESP_FAIL;
  }

  // Регистрируем обработчик событий
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  // Запускаем клиент
  ESP_LOGI(TAG, "Starting MQTT client");
  esp_err_t err = esp_mqtt_client_start(mqtt_client);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "MQTT client start error: %s", esp_err_to_name(err));
  } else {
    ESP_LOGI(TAG, "MQTT client started successfully");
  }
  return err;
}

// Функция для остановки MQTT клиента
void mqtt_stop(void) {
  if (mqtt_client) {
    ESP_LOGI(TAG, "Stopping MQTT client");
    esp_mqtt_client_stop(mqtt_client);
  }
}

esp_err_t mqtt_start(void) {
  esp_err_t err = ESP_FAIL; // Инициализируем значением по умолчанию
  // Запускаем клиент
  if (mqtt_client) {
    ESP_LOGI(TAG, "Starting MQTT client");
    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "MQTT client start error: %s", esp_err_to_name(err));
    } else {
      ESP_LOGI(TAG, "MQTT client started successfully");
    }
  } else {
    ESP_LOGE(TAG, "MQTT client is not initialized");
  }

  return err;
}
// Функция для публикации данных с DHT датчика
void mqtt_publish_dht_data(const dht_reading_t *dht_data) {
  // Проверяем, что MQTT подключен и данные действительны
  if (!dht_data || !dht_data->valid ||
      (uxWifiBits & FLAG_WIFI_CONNECTED) == 0 ||
      (uxMqttBits & FLAG_MQTT_CONNECTED) == 0) {
    ESP_LOGE(TAG, "Not publish DHT data status is disabled...");
    return;
  }

  // Создаем JSON объект
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    ESP_LOGE(TAG, "JSON object creation error");
    return;
  }

  // Добавляем данные
  cJSON_AddNumberToObject(root, "temperature", dht_data->temperature);
  cJSON_AddNumberToObject(root, "humidity", dht_data->humidity);

  // Преобразуем в строку
  char *json_string = cJSON_PrintUnformatted(root);
  if (json_string) {
    // Публикуем данные
    int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_DHT_DATA,
                                         json_string, 0, 0, 0);
    if (msg_id < 0) {
      ESP_LOGE(TAG, "DHT data publication error");
    } else {
      ESP_LOGI(TAG, "DHT data published: T=%.1f, H=%.1f", dht_data->temperature,
               dht_data->humidity);
    }

    // Освобождаем память
    free(json_string);
  }

  // Освобождаем JSON объект
  cJSON_Delete(root);
}

// Получение статуса MQTT соединения
bool is_mqtt_connected(void) { return uxMqttBits & FLAG_MQTT_CONNECTED; }