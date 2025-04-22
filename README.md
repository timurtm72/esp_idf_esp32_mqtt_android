# Мой сайт и резюме:

- [Мой сайт:](https://technocom.site123.me/)
- [Мое резюме инженер программист microcontrollers, PLC:](https://innopolis.hh.ru/resume/782d86d5ff0e9487200039ed1f6f3373384b30)
- [Мое резюме инженер программист Java backend developer (Spring):](https://innopolis.hh.ru/resume/9e3b451aff03fd23830039ed1f496e79587649)
- [Linkedin](https://www.linkedin.com/public-profile/settings?trk=d_flagship3_profile_self_view_public_profile)
  
# ESP32-S3 IoT Система Управления и Мониторинга

Встраиваемая система на базе ESP32-S3 для сбора данных с датчика температуры/влажности и управления RGB светодиодами через MQTT протокол. Проект реализован на ESP-IDF с использованием FreeRTOS.

## Архитектура проекта

Система построена на многозадачной архитектуре FreeRTOS и включает следующие компоненты:

### Задачи (Tasks)
- `dht_task` - чтение данных с DHT21/AM2301
- `led_task` - управление WS2812 RGB светодиодами 
- `process_task` - обработка и публикация данных

### Сервисы
- `wifi` - подключение к WiFi сети через DHCP
- `mqtt` - взаимодействие с MQTT брокером:
  - Публикация данных датчиков
  - Прием команд управления RGB
- `led_strip_encoder` - кодирование данных для WS2812

### Модели данных
```c
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
```

## Основные функции

### Работа с датчиком DHT
- Чтение температуры и влажности
- Фильтрация выбросов
- Сглаживание данных
- Обработка ошибок чтения
- Кэширование последних валидных значений

### Управление RGB
- Прием команд через MQTT
- Настройка цвета (RGB)
- Регулировка яркости
- Управление через RMT периферию

## Технические особенности
- Использование `EventGroups` для управления состояниями
- Очереди FreeRTOS для межзадачного взаимодействия
- Сторожевой таймер (WDT)
- JSON обработка через `cJSON`
- Автоматическое переподключение к WiFi/MQTT

## Потоки данных

### Входящие (MQTT)
```json
{
  "red": 255,
  "green": 0,
  "blue": 0,
  "brightness": 100
}
```

### Исходящие (MQTT)
```json
{
  "temperature": 25.6,
  "humidity": 45.2
}
```

## Структура проекта
```plaintext
main/
├── main.c          # Точка входа
├── main.h          # Общие определения
├── wifi.c          # WiFi функционал
├── mqtt.c          # MQTT клиент
├── dht.c           # Работа с DHT
├── led.c           # Управление RGB
├── process.c       # Обработка данных
└── led_strip_encoder.c # WS2812 энкодер
```

## Конфигурация
```c
// WiFi настройки
#define ESP_WIFI_SSID "SSID"
#define ESP_WIFI_PASSWORD "PASSWORD"

// MQTT настройки
#define MQTT_BROKER_URL "mqtt://broker"
#define MQTT_PORT 1883

// GPIO пины
#define DHT_GPIO 17
#define RMT_LED_STRIP_GPIO_NUM 48
```

## Требования
- ESP-IDF v5.0+
- ESP32-S3 DevKit
- Датчик DHT21/AM2301
- WS2812 RGB светодиоды

## Сборка и прошивка
1. Установить ESP-IDF
2. Клонировать репозиторий
3. Настроить параметры: `idf.py menuconfig`
4. Собрать проект: `idf.py build`
5. Прошить: `idf.py -p PORT flash`

## Связанные проекты
- [Flutter приложение](https://github.com/timurtm72/flutter_android_mqtt_python_esp32)
- [Python версия](https://github.com/timurtm72/python_mqtt_esp32_android)
- [Kotlin приложение](https://github.com/timurtm72/kotlin_mqtt_esp32_python)

## Особенности реализации
- Многоядерное исполнение (Core 0/1)
- Обработка ошибок и повторные попытки
- Энергоэффективное управление WiFi
- Оптимизированный протокол WS2812
- Защита от сбоев через WDT
