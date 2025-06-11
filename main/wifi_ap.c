#include "wifi_ap.h"
#include "main.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "WIFI_AP";
static httpd_handle_t server = NULL;

// HTML страница с формой для ввода Wi-Fi данных
static const char wifi_config_html[] = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"<meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
"<title>Параметры роутера</title>"
"<style>"
"* { margin: 0; padding: 0; box-sizing: border-box; }"
"body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; display: flex; align-items: center; justify-content: center; }"
".container { background: rgba(255,255,255,0.95); padding: 40px; border-radius: 20px; box-shadow: 0 15px 35px rgba(0,0,0,0.2); backdrop-filter: blur(10px); min-width: 320px; max-width: 400px; }"
"h1 { text-align: center; color: #333; margin-bottom: 30px; font-size: 28px; font-weight: 300; }"
".input-group { margin-bottom: 25px; }"
"label { display: block; margin-bottom: 8px; color: #555; font-weight: 500; font-size: 14px; }"
"input[type='text'], input[type='password'] { width: 100%; padding: 15px; border: 2px solid #e1e1e1; border-radius: 10px; font-size: 16px; transition: all 0.3s ease; background: #f8f9fa; }"
"input[type='text']:focus, input[type='password']:focus { outline: none; border-color: #667eea; background: white; box-shadow: 0 0 0 3px rgba(102,126,234,0.1); }"
"input[type='submit'] { width: 100%; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 15px; border: none; border-radius: 10px; cursor: pointer; font-size: 16px; font-weight: 600; transition: all 0.3s ease; text-transform: uppercase; letter-spacing: 1px; }"
"input[type='submit']:hover { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(102,126,234,0.4); }"
"input[type='submit']:active { transform: translateY(0); }"
"</style>"
"</head>"
"<body>"
"<div class='container'>"
"<h1>Параметры роутера</h1>"
"<form action='/save' method='post'>"
"<div class='input-group'>"
"<label for='ssid'>Имя сети</label>"
"<input type='text' id='ssid' name='ssid' required maxlength='32' placeholder='Введите имя WiFi сети'>"
"</div>"
"<div class='input-group'>"
"<label for='password'>Пароль</label>"
"<input type='password' id='password' name='password' required maxlength='64' placeholder='Введите пароль'>"
"</div>"
"<input type='submit' value='Подтвердить'>"
"</form>"
"</div>"
"</body>"
"</html>";

// Обработчик главной страницы
esp_err_t wifi_config_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, wifi_config_html, strlen(wifi_config_html));
}

// Функция для парсинга POST данных
esp_err_t parse_post_data(char *buf, char *ssid, char *password) {
    char *ssid_start = strstr(buf, "ssid=");
    char *password_start = strstr(buf, "password=");
    
    if (!ssid_start || !password_start) {
        return ESP_FAIL;
    }
    
    ssid_start += 5; // пропускаем "ssid="
    password_start += 9; // пропускаем "password="
    
    // Парсим SSID
    char *ssid_end = strchr(ssid_start, '&');
    if (ssid_end) {
        int ssid_len = ssid_end - ssid_start;
        strncpy(ssid, ssid_start, ssid_len);
        ssid[ssid_len] = '\0';
    } else {
        strcpy(ssid, ssid_start);
    }
    
    // Парсим password
    char *password_end = strchr(password_start, '&');
    if (password_end) {
        int password_len = password_end - password_start;
        strncpy(password, password_start, password_len);
        password[password_len] = '\0';
    } else {
        strcpy(password, password_start);
    }
    
    // URL декодирование (простое - заменяем %20 на пробел и + на пробел)
    char *ptr = ssid;
    while (*ptr) {
        if (*ptr == '+') *ptr = ' ';
        if (*ptr == '%' && *(ptr+1) == '2' && *(ptr+2) == '0') {
            *ptr = ' ';
            memmove(ptr+1, ptr+3, strlen(ptr+3)+1);
        }
        ptr++;
    }
    
    ptr = password;
    while (*ptr) {
        if (*ptr == '+') *ptr = ' ';
        if (*ptr == '%' && *(ptr+1) == '2' && *(ptr+2) == '0') {
            *ptr = ' ';
            memmove(ptr+1, ptr+3, strlen(ptr+3)+1);
        }
        ptr++;
    }
    
    return ESP_OK;
}

// Обработчик сохранения настроек
esp_err_t wifi_save_handler(httpd_req_t *req) {
    char buf[256];
    wifi_credentials_t credentials = {0};
    
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    if (parse_post_data(buf, credentials.ssid, credentials.password) != ESP_OK) {
        const char error_resp[] = "<html><body><h1>Ошибка обработки данных</h1><a href='/'>Назад</a></body></html>";
        httpd_resp_send(req, error_resp, strlen(error_resp));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Received data: SSID='%s', Password='%s'", credentials.ssid, credentials.password);
    
    // Сохраняем в NVS
    esp_err_t err = save_wifi_credentials(&credentials);
    if (err != ESP_OK) {
        const char error_resp[] = "<html><body><h1>Ошибка сохранения настроек</h1><a href='/'>Назад</a></body></html>";
        httpd_resp_send(req, error_resp, strlen(error_resp));
        return ESP_FAIL;
    }
    
    const char success_resp[] = 
    "<html><body>"
    "<h1>Настройки сохранены!</h1>"
    "<p>Контроллер перезагрузится через 3 секунды для подключения к новой сети.</p>"
    "<script>setTimeout(function(){window.location='/'}, 3000);</script>"
    "</body></html>";
    
    httpd_resp_send(req, success_resp, strlen(success_resp));
    
    // Перезагрузка через 3 секунды
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    
    return ESP_OK;
}

// Функция для сохранения Wi-Fi данных в NVS
esp_err_t save_wifi_credentials(const wifi_credentials_t *credentials) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_str(nvs_handle, "ssid", credentials->ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_set_str(nvs_handle, "password", credentials->password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    return err;
}

// Функция для загрузки Wi-Fi данных из NVS
esp_err_t load_wifi_credentials(wifi_credentials_t *credentials) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size;
    
    err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS not found or corrupted: %s", esp_err_to_name(err));
        return err;
    }
    
    // Читаем SSID
    required_size = sizeof(credentials->ssid);
    err = nvs_get_str(nvs_handle, "ssid", credentials->ssid, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SSID not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    // Читаем пароль
    required_size = sizeof(credentials->password);
    err = nvs_get_str(nvs_handle, "password", credentials->password, &required_size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Password not found in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WiFi credentials loaded from NVS: SSID='%s'", credentials->ssid);
    return ESP_OK;
}

// Функция для очистки Wi-Fi данных
esp_err_t clear_wifi_credentials(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    nvs_erase_all(nvs_handle);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "WiFi credentials cleared");
    return ESP_OK;
}

// Запуск веб-сервера
esp_err_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Регистрируем обработчики
        httpd_uri_t index_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = wifi_config_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_uri);
        
        httpd_uri_t save_uri = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = wifi_save_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &save_uri);
        
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Error starting HTTP server!");
    return ESP_FAIL;
}

// Остановка веб-сервера
void stop_webserver(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

// Инициализация Access Point
esp_err_t wifi_init_ap(void) {
    ESP_LOGI(TAG, "Initializing WiFi in Access Point mode...");
    
    if (current_mode != WIFI_MODE_NULL) {
        esp_wifi_stop();
        esp_wifi_deinit();
    }
    
    if (!ap_netif) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    if (strlen(WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    current_mode = WIFI_MODE_AP;
    
    ESP_LOGI(TAG, "Access Point started. SSID: %s", WIFI_AP_SSID);
    
    return start_webserver();
}

// Проверка наличия сохраненных Wi-Fi данных
bool has_saved_wifi_credentials(void) {
    wifi_credentials_t credentials;
    return (load_wifi_credentials(&credentials) == ESP_OK);
} 