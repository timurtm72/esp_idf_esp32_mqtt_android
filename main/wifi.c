#include "main.h"
#include "wifi_ap.h"

static const char *TAG = "WIFI";
esp_netif_t *sta_netif = NULL;
esp_netif_t *ap_netif = NULL;
EventGroupHandle_t wifi_event_group;
EventBits_t uxWifiBits;
static int s_retry_num = 0;
wifi_mode_t current_mode = WIFI_MODE_NULL;

#define MAX_RETRY_COUNT 5

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                       int32_t event_id, void *event_data);

void wifi_init(void) {
  wifi_event_group = xEventGroupCreate();
  ESP_LOGI(TAG, "Initializing WiFi...");
  
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));
  
  wifi_credentials_t credentials;
  
  if (has_saved_wifi_credentials() && load_wifi_credentials(&credentials) == ESP_OK) {
    ESP_LOGI(TAG, "Found saved WiFi credentials, connecting to: %s", credentials.ssid);
    wifi_init_sta_mode(credentials.ssid, credentials.password);
  } else {
    ESP_LOGI(TAG, "No saved WiFi credentials found, starting AP mode");
    wifi_init_ap();
  }
}

void wifi_init_sta_mode(const char *ssid, const char *password) {
  if (current_mode != WIFI_MODE_NULL) {
    esp_wifi_stop();
    esp_wifi_deinit();
  }
  
  if (!sta_netif) {
    sta_netif = esp_netif_create_default_wifi_sta();
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  wifi_config_t wifi_config = {0};
  strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_config.sta.failure_retry_cnt = 0;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  
  current_mode = WIFI_MODE_STA;
  ESP_LOGI(TAG, "WiFi STA initialization completed, connecting to %s", ssid);
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                       int32_t event_id, void *event_data) {
  if (wifi_event_group) {
    uxWifiBits = xEventGroupGetBits(wifi_event_group);
  }
  
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "WiFi STA started, connecting...");
      esp_wifi_connect();
      s_retry_num = 0;
      break;

    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "Connected to WiFi network");
      xEventGroupSetBits(wifi_event_group, FLAG_WIFI_CONNECTED);
      s_retry_num = 0;
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      s_retry_num++;
      ESP_LOGI(TAG, "Disconnected from AP, attempt %d/%d", s_retry_num, MAX_RETRY_COUNT);
      
      xEventGroupClearBits(wifi_event_group, FLAG_WIFI_CONNECTED);
      
      if (mqtt_client) {
        ESP_LOGI(TAG, "Stopping MQTT due to WiFi disconnection");
        mqtt_stop();
      }
      
      if (s_retry_num < MAX_RETRY_COUNT) {
        esp_wifi_connect();
      } else {
        ESP_LOGE(TAG, "Failed to connect to WiFi after %d attempts", MAX_RETRY_COUNT);
        ESP_LOGI(TAG, "Switching to AP mode...");
        wifi_init_ap();
      }
      break;
      
    case WIFI_EVENT_AP_STACONNECTED: {
      wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *) event_data;
      ESP_LOGI(TAG, "Station connected to AP, MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
               event->mac[0], event->mac[1], event->mac[2], 
               event->mac[3], event->mac[4], event->mac[5]);
      break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
      wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *) event_data;
      ESP_LOGI(TAG, "Station disconnected from AP, MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
               event->mac[0], event->mac[1], event->mac[2], 
               event->mac[3], event->mac[4], event->mac[5]);
      break;
    }
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    if (sta_netif) {
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&ip_info.ip));
      }
    }

    if ((uxWifiBits & FLAG_WIFI_CONNECTED) &&
        ((uxMqttBits & FLAG_MQTT_CONNECTED) == 0)) {
      ESP_LOGI(TAG, "Starting MQTT after WiFi connection");
      esp_err_t result = mqtt_init();
      if (result == ESP_OK) {
        ESP_LOGI(TAG, "MQTT started successfully");
      } else {
        ESP_LOGE(TAG, "Failed to start MQTT: %s", esp_err_to_name(result));
      }
    }
  }
}