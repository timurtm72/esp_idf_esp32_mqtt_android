# Мой сайт и резюме:

- [Мой сайт:](https://technocom.site123.me/)
- [Мое резюме инженер программист microcontrollers, PLC:](https://innopolis.hh.ru/resume/782d86d5ff0e9487200039ed1f6f3373384b30)
- [Мое резюме инженер программист Java backend developer (Spring):](https://innopolis.hh.ru/resume/9e3b451aff03fd23830039ed1f496e79587649)
- [Linkedin](https://www.linkedin.com/public-profile/settings?trk=d_flagship3_profile_self_view_public_profile)
  
# ESP32 IoT Project с веб-настройкой Wi-Fi

IoT проект на ESP32 с датчиком DHT, RGB светодиодом, MQTT клиентом и веб-интерфейсом для настройки Wi-Fi через Access Point режим.

## Возможности

- 📡 **Автоматическое переключение Wi-Fi режимов**: STA ↔ AP
- 🌐 **Веб-интерфейс настройки Wi-Fi** через браузер телефона
- 💾 **Сохранение настроек** в энергонезависимой памяти (NVS)
- 🌡️ **Датчик DHT** (температура и влажность)
- 💡 **RGB светодиод** с изменением цвета по температуре
- 📊 **MQTT клиент** для отправки данных
- 🔄 **Автоматическое восстановление** подключения

## Аппаратные требования

- **ESP32** (любая плата)
- **DHT22/AM2301** датчик (подключен к GPIO 17)
- **RGB светодиод WS2812** (подключен к GPIO 48)
- **Питание** 5V через USB или внешний блок питания

## Схема подключения

```
ESP32 GPIO 17  → DHT22 Data
ESP32 GPIO 48  → WS2812 DIN
ESP32 3.3V     → DHT22 VCC
ESP32 5V       → WS2812 VCC
ESP32 GND      → DHT22 GND, WS2812 GND
```

## Сборка и прошивка

### 1. Установка ESP-IDF

```bash
# Установи ESP-IDF v5.1 или новее
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
. ./export.sh
```

### 2. Клонирование проекта

```bash
git clone <your-repo-url>
cd esp32-iot-project
```

### 3. Настройка проекта

```bash
# Настрой конфигурацию проекта
idf.py menuconfig
```

Важные настройки в menuconfig:
- `Component config → Wi-Fi → WiFi Task Core ID` = `0`
- `Component config → FreeRTOS → Run FreeRTOS only on first core` = `Enable`

### 4. Сборка и прошивка

```bash
# Собери проект
idf.py build

# Прошей ESP32 (замени COM-порт на свой)
idf.py -p COM3 flash monitor
```

## Использование

### Первый запуск

1. **ESP32 запустится в AP режиме** (так как нет сохраненных Wi-Fi данных)
2. **Найди сеть Wi-Fi**: `ESP32-Config` (пароль: `12345678`)
3. **Подключись к ней** с телефона или компьютера
4. **Открой браузер** и перейди по адресу: `http://192.168.4.1`

### Настройка Wi-Fi

1. **Увидишь веб-форму** с полями:
   - Имя сети (SSID)
   - Пароль
2. **Введи данные** своего роутера
3. **Нажми "Подключиться"**
4. **ESP32 перезагрузится** через 3 секунды
5. **Подключится к твоей сети** и начнет работу

### Обычная работа

После успешного подключения к Wi-Fi:
- 🌡️ **Читает данные** с DHT каждые 3 секунды
- 💡 **Меняет цвет RGB** в зависимости от температуры:
  - 🔵 Синий: < 20°C
  - 🟢 Зеленый: 20-25°C  
  - 🟡 Желтый: 25-30°C
  - 🔴 Красный: > 30°C
- 📊 **Отправляет данные** в MQTT брокер
- 📱 **Показывает статус** через серийный порт

### Сброс настроек

Если нужно сменить Wi-Fi сеть:
1. **Отключи ESP32** от питания на 5+ секунд
2. **Либо используй функцию** `clear_wifi_credentials()` в коде
3. **ESP32 снова запустится** в AP режиме

## Настройки

### Wi-Fi Access Point (в `main/wifi_ap.h`)

```c
#define WIFI_AP_SSID "ESP32-Config"      // Имя AP сети
#define WIFI_AP_PASSWORD "12345678"      // Пароль AP (пустой = открытая сеть)
#define WIFI_AP_CHANNEL 1                // Канал Wi-Fi
#define WIFI_AP_MAX_CONNECTIONS 4        // Макс. подключений
```

### MQTT настройки (в `main/main.h`)

```c
#define MQTT_BROKER_URL "mqtt://193.43.147.210"
#define MQTT_PORT 1883
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
```

### DHT датчик (в `main/main.h`)

```c
#define DHT_GPIO 17                      // GPIO пин датчика
#define DHT_TYPE DHT_TYPE_AM2301         // Тип датчика
#define DHT_READ_INTERVAL 3000           // Интервал чтения (мс)
```

### RGB светодиод (в `main/main.h`)

```c
#define RMT_LED_STRIP_GPIO_NUM 48        // GPIO пин светодиода
#define EXAMPLE_LED_NUMBERS 1            // Количество светодиодов
```

## Структура проекта

```
esp32-iot-project/
├── main/
│   ├── main.c           # Главный файл
│   ├── main.h           # Заголовки и настройки
│   ├── wifi.c           # Wi-Fi STA режим
│   ├── wifi_ap.c        # Wi-Fi AP режим + веб-сервер
│   ├── wifi_ap.h        # Заголовки AP режима
│   ├── dht.c            # Работа с DHT датчиком
│   ├── led.c            # Управление RGB светодиодом
│   ├── mqtt.c           # MQTT клиент
│   ├── process.c        # Обработка данных
│   └── CMakeLists.txt   # Настройки сборки
├── components/          # Внешние компоненты
├── CMakeLists.txt       # Корневые настройки сборки
├── sdkconfig           # Конфигурация ESP-IDF
└── README.md           # Этот файл
```

## API функции

### Wi-Fi управление

```c
void wifi_init(void);                                    // Автоинициализация Wi-Fi
void wifi_init_sta_mode(const char *ssid, const char *password); // STA режим
esp_err_t wifi_init_ap(void);                           // AP режим

bool has_saved_wifi_credentials(void);                  // Проверка сохраненных данных
esp_err_t save_wifi_credentials(const char *ssid, const char *password); // Сохранение
esp_err_t load_wifi_credentials(char *ssid, char *password);              // Загрузка
esp_err_t clear_wifi_credentials(void);                 // Очистка
```

### Веб-сервер

```c
esp_err_t start_webserver(void);                        // Запуск сервера
void stop_webserver(void);                              // Остановка сервера
```

## Отладка

### Просмотр логов

```bash
idf.py monitor
```

### Типичные ошибки

1. **ESP32 не подключается к Wi-Fi**
   - Проверь правильность SSID и пароля
   - Убедись что роутер в зоне действия
   - Проверь тип шифрования (поддерживается WPA/WPA2)

2. **Не открывается веб-страница**
   - Убедись что подключен к сети `ESP32-Config`
   - Попробуй адрес `192.168.4.1` напрямую
   - Отключи мобильные данные на телефоне

3. **DHT показывает некорректные данные**
   - Проверь подключение к GPIO 17
   - Убедись в питании 3.3V для датчика
   - Попробуй другой датчик

4. **RGB не работает**
   - Проверь подключение к GPIO 48
   - Убедись в питании 5V для WS2812
   - Проверь тип светодиода в настройках

## Лицензия

MIT License

## Поддержка

Если возникли вопросы:
1. Проверь логи через `idf.py monitor`
2. Убедись в правильности подключения датчиков
3. Проверь настройки в `main.h`

---
Создано для ESP-IDF v5.1+
