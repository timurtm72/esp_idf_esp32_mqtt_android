#include "main.h"

static const char *TAG = "led_task";

QueueHandle_t qLedQueue;
TaskHandle_t thLedHandle = NULL;

void led_task(void *pvParameter) {

  qLedQueue = xQueueCreate(10, sizeof(rgb_led_values_t));
  rgb_led_values_t rgb_led_values = {
      .red = 0, // Инициализируем красный цвет нулем
      .green = 0, // Инициализируем зеленый цвет нулем
      .blue = 0, // Инициализируем синий цвет нулем
      .brightness = 255 // Инициализируем яркость на 100%
  };

  ESP_LOGI(TAG, "Create RMT TX channel");
  rmt_channel_handle_t led_chan = NULL;
  rmt_tx_channel_config_t tx_chan_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .gpio_num = RMT_LED_STRIP_GPIO_NUM,
      .mem_block_symbols = 64,
      .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
      .trans_queue_depth = 4,
  };
  ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

  ESP_LOGI(TAG, "Install led strip encoder");
  rmt_encoder_handle_t led_encoder = NULL;
  led_strip_encoder_config_t encoder_config = {
      .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
  };
  ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

  ESP_LOGI(TAG, "Enable RMT TX channel");
  ESP_ERROR_CHECK(rmt_enable(led_chan));

  ESP_LOGI(TAG, "Start LED color cycle");
  rmt_transmit_config_t tx_config = {
      .loop_count = 0,
  };

  // Буфер для одного светодиода (3 байта: G, R, B)
  uint8_t led_data[3] = {0};
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20);
  while (1) {
    if (uxQueueMessagesWaiting(qLedQueue) > 0) {
      xQueueReceive(qLedQueue, &rgb_led_values, portMAX_DELAY);
    }
    led_data[0] = (rgb_led_values.green * rgb_led_values.brightness) / 255.0;
    led_data[1] = (rgb_led_values.red * rgb_led_values.brightness) / 255.0;
    led_data[2] = (rgb_led_values.blue * rgb_led_values.brightness) / 255.0;

    // Отправляем данные на светодиод
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_data,
                                 sizeof(led_data), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

    // Задержка до следующего интервала
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void start_led_task() {
  xTaskCreatePinnedToCore(led_task,   // Функция задачи
                          "led_task", // Имя задачи для отладки
                          4096,       // Размер стека в словах
                          NULL,       // Параметры задачи
                          5,          // Приоритет задачи
                          NULL, // Указатель на хендл задачи
                          1     // Номер ядра (0 или 1)
  );
}
