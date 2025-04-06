#include "main.h"

// Определение логового тега
static const char *TAG = "process_task";

QueueHandle_t qProcessQueue;
TaskHandle_t thProcessHandle = NULL;

void process_start_task() {
  xTaskCreatePinnedToCore(process_task, // Функция задачи
                          "process_task", // Имя задачи для отладки
                          4096, // Размер стека в словах
                          NULL, // Параметры задачи
                          0,    // Приоритет задачи
                          &thProcessHandle, // Указатель на хендл задачи
                          1                 // Номер ядра (1)
  );
}

void process_task(void *pvParameter) {
  rgb_led_values_t rgb_led_values_to_send = {
      .red = 0, .green = 0, .blue = 0, .brightness = 0xFF};
  dht_reading_t dht_reading = {.humidity = 0, .temperature = 0, .valid = false};
  
  while (1) {
    xQueueSend(qLedQueue, &rgb_led_values_to_send, (TickType_t)0);
    if (uxQueueMessagesWaiting(qDhtQueue) > 0) {
      xQueueReceive(qDhtQueue, &dht_reading, portMAX_DELAY);
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}