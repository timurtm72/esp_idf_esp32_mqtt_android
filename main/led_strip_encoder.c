#include "led_strip_encoder.h"
#include "esp_check.h"
#include "main.h"
static const char *TAG = "led_task";

QueueHandle_t qLedQueue;
TaskHandle_t thLedHandle = NULL;

// static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

typedef struct {
  rmt_encoder_t base;
  rmt_encoder_t *bytes_encoder;
  rmt_encoder_t *copy_encoder;
  int state;
  rmt_symbol_word_t reset_code;
} rmt_led_strip_encoder_t;

static size_t rmt_encode_led_strip(rmt_encoder_t *encoder,
                                   rmt_channel_handle_t channel,
                                   const void *primary_data, size_t data_size,
                                   rmt_encode_state_t *ret_state) {
  rmt_led_strip_encoder_t *led_encoder =
      __containerof(encoder, rmt_led_strip_encoder_t, base);
  rmt_encoder_handle_t bytes_encoder = led_encoder->bytes_encoder;
  rmt_encoder_handle_t copy_encoder = led_encoder->copy_encoder;
  rmt_encode_state_t session_state = RMT_ENCODING_RESET;
  rmt_encode_state_t state = RMT_ENCODING_RESET;
  size_t encoded_symbols = 0;
  switch (led_encoder->state) {
  case 0: // send RGB data
    encoded_symbols += bytes_encoder->encode(
        bytes_encoder, channel, primary_data, data_size, &session_state);
    if (session_state & RMT_ENCODING_COMPLETE) {
      led_encoder->state =
          1; // switch to next state when current encoding session finished
    }
    if (session_state & RMT_ENCODING_MEM_FULL) {
      state |= RMT_ENCODING_MEM_FULL;
      goto out; // yield if there's no free space for encoding artifacts
    }
  // fall-through
  case 1: // send reset code
    encoded_symbols +=
        copy_encoder->encode(copy_encoder, channel, &led_encoder->reset_code,
                             sizeof(led_encoder->reset_code), &session_state);
    if (session_state & RMT_ENCODING_COMPLETE) {
      led_encoder->state =
          RMT_ENCODING_RESET; // back to the initial encoding session
      state |= RMT_ENCODING_COMPLETE;
    }
    if (session_state & RMT_ENCODING_MEM_FULL) {
      state |= RMT_ENCODING_MEM_FULL;
      goto out; // yield if there's no free space for encoding artifacts
    }
  }
out:
  *ret_state = state;
  return encoded_symbols;
}

static esp_err_t rmt_del_led_strip_encoder(rmt_encoder_t *encoder) {
  rmt_led_strip_encoder_t *led_encoder =
      __containerof(encoder, rmt_led_strip_encoder_t, base);
  rmt_del_encoder(led_encoder->bytes_encoder);
  rmt_del_encoder(led_encoder->copy_encoder);
  free(led_encoder);
  return ESP_OK;
}

static esp_err_t rmt_led_strip_encoder_reset(rmt_encoder_t *encoder) {
  rmt_led_strip_encoder_t *led_encoder =
      __containerof(encoder, rmt_led_strip_encoder_t, base);
  rmt_encoder_reset(led_encoder->bytes_encoder);
  rmt_encoder_reset(led_encoder->copy_encoder);
  led_encoder->state = RMT_ENCODING_RESET;
  return ESP_OK;
}

esp_err_t rmt_new_led_strip_encoder(const led_strip_encoder_config_t *config,
                                    rmt_encoder_handle_t *ret_encoder) {
  esp_err_t ret = ESP_OK;
  rmt_led_strip_encoder_t *led_encoder = NULL;
  ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG,
                    "invalid argument");
  led_encoder = rmt_alloc_encoder_mem(sizeof(rmt_led_strip_encoder_t));
  ESP_GOTO_ON_FALSE(led_encoder, ESP_ERR_NO_MEM, err, TAG,
                    "no mem for led strip encoder");
  led_encoder->base.encode = rmt_encode_led_strip;
  led_encoder->base.del = rmt_del_led_strip_encoder;
  led_encoder->base.reset = rmt_led_strip_encoder_reset;
  // different led strip might have its own timing requirements, following
  // parameter is for WS2812
  rmt_bytes_encoder_config_t bytes_encoder_config = {
      .bit0 =
          {
              .level0 = 1,
              .duration0 = 0.3 * config->resolution / 1000000, // T0H=0.3us
              .level1 = 0,
              .duration1 = 0.9 * config->resolution / 1000000, // T0L=0.9us
          },
      .bit1 =
          {
              .level0 = 1,
              .duration0 = 0.9 * config->resolution / 1000000, // T1H=0.9us
              .level1 = 0,
              .duration1 = 0.3 * config->resolution / 1000000, // T1L=0.3us
          },
      .flags.msb_first = 1 // WS2812 transfer bit order: G7...G0R7...R0B7...B0
  };
  ESP_GOTO_ON_ERROR(
      rmt_new_bytes_encoder(&bytes_encoder_config, &led_encoder->bytes_encoder),
      err, TAG, "create bytes encoder failed");
  rmt_copy_encoder_config_t copy_encoder_config = {};
  ESP_GOTO_ON_ERROR(
      rmt_new_copy_encoder(&copy_encoder_config, &led_encoder->copy_encoder),
      err, TAG, "create copy encoder failed");

  uint32_t reset_ticks = config->resolution / 1000000 * 50 /
                         2; // reset code duration defaults to 50us
  led_encoder->reset_code = (rmt_symbol_word_t){
      .level0 = 0,
      .duration0 = reset_ticks,
      .level1 = 0,
      .duration1 = reset_ticks,
  };
  *ret_encoder = &led_encoder->base;
  return ESP_OK;
err:
  if (led_encoder) {
    if (led_encoder->bytes_encoder) {
      rmt_del_encoder(led_encoder->bytes_encoder);
    }
    if (led_encoder->copy_encoder) {
      rmt_del_encoder(led_encoder->copy_encoder);
    }
    free(led_encoder);
  }
  return ret;
}

void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r,
                       uint32_t *g, uint32_t *b) {
  h %= 360; // h -> [0,360]
  uint32_t rgb_max = v * 2.55f;
  uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

  uint32_t i = h / 60;
  uint32_t diff = h % 60;

  // RGB adjustment amount by hue
  uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

  switch (i) {
  case 0:
    *r = rgb_max;
    *g = rgb_min + rgb_adj;
    *b = rgb_min;
    break;
  case 1:
    *r = rgb_max - rgb_adj;
    *g = rgb_max;
    *b = rgb_min;
    break;
  case 2:
    *r = rgb_min;
    *g = rgb_max;
    *b = rgb_min + rgb_adj;
    break;
  case 3:
    *r = rgb_min;
    *g = rgb_max - rgb_adj;
    *b = rgb_max;
    break;
  case 4:
    *r = rgb_min + rgb_adj;
    *g = rgb_min;
    *b = rgb_max;
    break;
  default:
    *r = rgb_max;
    *g = rgb_min;
    *b = rgb_max - rgb_adj;
    break;
  }
}
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
  // static uint8_t state = 0;
  // const uint8_t bridgest = 32;
  while (1) {
    // Вычисляем цвет из HSV
    // led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);

    /*switch (state) {
    case 0:
      if ((green++) >= bridgest) {
        green = bridgest;
        state = 1;
      }
      break;
    case 1:
      green--;
      if (green == 0x00) {
        green = 0x00;
        state = 2;
      }
      break;
    case 2:
      if ((red++) >= bridgest) {
        red = bridgest;
        state = 3;
      }
      break;
    case 3:
      red--;
      if (red == 0x00) {
        red = 0x00;
        state = 4;
      }
      break;
    case 4:
      if ((blue++) >= 0xFF) {
        blue = bridgest;
        state = 5;
      }
      break;
    case 5:
    blue--;
      if (blue == 0x00) {
        blue = 0;
        state = 0;
      }
      break;
    }*/

    if (uxQueueMessagesWaiting(qLedQueue) > 0) {
      xQueueReceive(qLedQueue, &rgb_led_values, portMAX_DELAY);
    }

    led_data[0] =
        scale(rgb_led_values.green, 0, 100, 0, rgb_led_values.brightness);
    led_data[1] =
        scale(rgb_led_values.red, 0, 100, 0, rgb_led_values.brightness);
    led_data[2] =
        scale(rgb_led_values.blue, 0, 100, 0, rgb_led_values.brightness);

    // Отправляем данные на светодиод
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_data,
                                 sizeof(led_data), &tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));

    // Плавно меняем цвет
    // hue = (hue + 1) % 360;

    // Небольшая задержка для визуализации изменения
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void led_start_task() {
  xTaskCreatePinnedToCore(led_task,   	// Функция задачи
                          "led_task", 		// Имя задачи для отладки
                          4096,       // Размер стека в словах
                          NULL,       // Параметры задачи
                          5,          	// Приоритет задачи
                          NULL, 		// Указатель на хендл задачи
                          1     			// Номер ядра (0 или 1)
  );
}
