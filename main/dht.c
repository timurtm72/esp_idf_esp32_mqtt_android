#include "main.h"

// Определение логового тега
static const char *TAG = "dht_task";

QueueHandle_t qDhtQueue;
TaskHandle_t thDhtHandle = NULL;

dht_reading_t last_valid_reading = {
    .temperature = 0, .humidity = 0, .valid = false};
int failed_reads_count = 0;

// Проверка валидности значений
bool is_dht_reading_valid(float humidity, float temperature) {
  return (humidity >= DHT_HUM_MIN && humidity <= DHT_HUM_MAX &&
          temperature >= DHT_TEMP_MIN && temperature <= DHT_TEMP_MAX);
}
// Инициализация DHT
esp_err_t init_dht() {
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << DHT_GPIO),
      .mode = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };

  esp_err_t ret = gpio_config(&io_conf);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
  }
  return ret;
}

void dht_start_task() {  // Создание задачи для работы с DHT сенсором, привязанной к ядру 0
  xTaskCreatePinnedToCore(dht_task,   // Функция задачи
                          "dht_task", // Имя задачи для отладки
                          4096,       // Размер стека в словах
                          NULL,       // Параметры задачи
                          5,          // Приоритет задачи
                          &thDhtHandle, // Указатель на хендл задачи
                          0     // Номер ядра (0 или 1)
  );  
}

void dht_task(void *pvParameter) {
  esp_task_wdt_user_handle_t wdtUserHandler;
  qDhtQueue = xQueueCreate(10, sizeof(dht_reading_t));

  // Регистрируем задачу в сторожевом таймере (только один раз!)
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
  ESP_ERROR_CHECK(esp_task_wdt_add_user("resetWdt", &wdtUserHandler));

  // Проверяем статус WDT
  if (esp_task_wdt_status(NULL) == !ESP_OK) {
    // WDT не инициализирован, инициализируем его
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000, // Тайм-аут 10 секунд
        .idle_core_mask = 0, // Не отслеживать простаивающие ядра
        .trigger_panic = true // Вызывать панику при срабатывании
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));
    ESP_LOGI(TAG, "WDT not initialized");
  } else {
    ESP_LOGI(TAG, "WDT already initialized");
  }

  // Инициализация DHT
  if (init_dht() != ESP_OK) {
    ESP_LOGE(TAG, "DHT sensor initialization error. Task stopped");
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
    return;
  }

  float humidity = 0, temperature = 0;
  esp_err_t result;

  while (1) {
    // Попытки чтения с несколькими повторами при ошибке
    bool read_success = false;

    // Сбрасываем таймер WDT в начале каждой итерации
    esp_task_wdt_reset();
    esp_task_wdt_reset_user(wdtUserHandler);

    for (int retry = 0; retry < DHT_MAX_RETRIES && !read_success; retry++) {
      result = dht_read_float_data(DHT_TYPE, DHT_GPIO, &humidity, &temperature);

      if (result == ESP_OK && is_dht_reading_valid(humidity, temperature)) {
        read_success = true;
        failed_reads_count = 0;

        // Фильтрация выбросов (скачков более 10 единиц)
        if (last_valid_reading.valid) {
          float temp_diff = fabsf(temperature - last_valid_reading.temperature);
          float hum_diff = fabsf(humidity - last_valid_reading.humidity);

          if (temp_diff > 10.0 || hum_diff > 10.0) {
            ESP_LOGW(TAG, "Value jump detected: T: %.1f→%.1f, H: %.1f→%.1f",
                     last_valid_reading.temperature, temperature,
                     last_valid_reading.humidity, humidity);

            // Сглаживание
            temperature = (temperature + last_valid_reading.temperature) / 2;
            humidity = (humidity + last_valid_reading.humidity) / 2;
          }
        }

        // Сохранение валидных значений
        last_valid_reading.temperature = temperature;
        last_valid_reading.humidity = humidity;
        last_valid_reading.valid = true;

        ESP_LOGI(TAG, "Humidity: %.1f%% Temperature: %.1fC", humidity,
                 temperature);
      } else {
        ESP_LOGW(TAG, "Attempt %d: Reading error: %s", retry + 1,
                 esp_err_to_name(result));
        vTaskDelay(
            pdMS_TO_TICKS(500)); // Короткая пауза перед повторной попыткой
      }
    }

    if (!read_success) {
      failed_reads_count++;
      ESP_LOGE(
          TAG,
          "Failed to read data after %d attempts. Total consecutive errors: %d",
          DHT_MAX_RETRIES, failed_reads_count);

      // Если есть последние валидные значения - используем их
      if (last_valid_reading.valid) {
        ESP_LOGI(TAG, "Using last valid values: T: %.1fC, H: %.1f%%",
                 last_valid_reading.temperature, last_valid_reading.humidity);
      }

      // Если много ошибок подряд - возможно, проблема с датчиком
      if (failed_reads_count > 10) {
        ESP_LOGE(TAG,
                 "Too many consecutive reading errors, possible sensor issue");
      }
    }
	xQueueSend(qDhtQueue, &last_valid_reading, (TickType_t )0); 
    vTaskDelay(pdMS_TO_TICKS(DHT_READ_INTERVAL));
  }
}
