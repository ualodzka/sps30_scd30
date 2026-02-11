#include <Arduino.h>                // базовая библиотека Arduino
#include <SensirionI2cSps30.h>      // библиотека для датчика пыли SPS30
#include <Wire.h>                   // библиотека для I2C (провода SDA/SCL)
#include <WiFiS3.h>                 // WiFi для Arduino R4 WiFi
#include <PubSubClient.h>           // MQTT клиент

#include "SparkFun_SCD30_Arduino_Library.h"  // библиотека для датчика CO2 SCD30
#include "secrets.h"                // WiFi credentials (не в git!)

#ifdef NO_ERROR                     // если NO_ERROR уже определён где-то в библиотеках —
#undef NO_ERROR                     // удаляем старое определение,
#endif
#define NO_ERROR 0                  // и задаём своё: 0 = нет ошибки

// ===== MQTT настройки =====
const char* MQTT_SERVER = "broker.hivemq.com";  // публичный тестовый брокер
// const char* MQTT_SERVER = "192.168.1.12";    // IP адрес компьютера с Home Assistant
const int MQTT_PORT = 1883;                      // порт MQTT брокера

// Уникальный ID устройства (чтобы не пересекаться с другими на публичном брокере)
#define DEVICE_ID "levl_x7k9m2"

// MQTT топики для Home Assistant (автообнаружение)
const char* MQTT_TOPIC_PM25 = DEVICE_ID "/sensor/pm25/state";
const char* MQTT_TOPIC_CO2 = DEVICE_ID "/sensor/co2/state";
const char* MQTT_TOPIC_TEMP = DEVICE_ID "/sensor/temperature/state";
const char* MQTT_TOPIC_HUMIDITY = DEVICE_ID "/sensor/humidity/state";

// Топики для автоконфигурации Home Assistant (уникальный префикс!)
#define DISCOVERY_PREFIX DEVICE_ID "_ha"
const char* MQTT_CONFIG_PM25 = DISCOVERY_PREFIX "/sensor/" DEVICE_ID "_pm25/config";
const char* MQTT_CONFIG_CO2 = DISCOVERY_PREFIX "/sensor/" DEVICE_ID "_co2/config";
const char* MQTT_CONFIG_TEMP = DISCOVERY_PREFIX "/sensor/" DEVICE_ID "_temp/config";
const char* MQTT_CONFIG_HUMIDITY = DISCOVERY_PREFIX "/sensor/" DEVICE_ID "_humidity/config";

WiFiClient wifiClient;              // WiFi клиент
PubSubClient mqtt(wifiClient);      // MQTT клиент поверх WiFi

SensirionI2cSps30 sps;              // объект для работы с датчиком пыли SPS30
SCD30 airSensor;                    // объект для работы с датчиком CO2 SCD30

static char errorMessage[64];       // буфер для текста ошибки (макс 64 символа)
static int16_t error;               // код ошибки, возвращаемый функциями SPS30
uint32_t auto_clean_interval = 4 * 24 * 3600;  // интервал автоочистки вентилятора: 4 дня в секундах

// Подключение к WiFi
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Ждём подключения к WiFi
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection FAILED!");
    return;
  }

  // Ждём получения IP адреса (не 0.0.0.0)
  Serial.print("Waiting for IP");
  attempts = 0;
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to get IP address!");
  }
}

// Подключение к MQTT брокеру
void connectMQTT() {
  Serial.print("MQTT server: ");
  Serial.print(MQTT_SERVER);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  // Тест TCP соединения
  Serial.print("Testing TCP connection... ");
  WiFiClient testClient;
  if (testClient.connect(MQTT_SERVER, MQTT_PORT)) {
    Serial.println("TCP OK!");
    testClient.stop();
  } else {
    Serial.println("TCP FAILED!");
  }

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setKeepAlive(60);

  Serial.print("Connecting to MQTT");
  int attempts = 0;
  while (!mqtt.connected() && attempts < 5) {
    Serial.print(".");
    if (mqtt.connect(DEVICE_ID)) {
      Serial.println(" connected!");

      // Отправляем конфигурацию для автообнаружения Home Assistant
      publishHomeAssistantConfig();
    } else {
      int state = mqtt.state();
      Serial.print(" error=");
      Serial.print(state);
      delay(2000);
      attempts++;
    }
  }

  if (!mqtt.connected()) {
    Serial.println(" FAILED!");
  }
}

// Публикация конфигурации для автообнаружения Home Assistant
void publishHomeAssistantConfig() {
  char configPayload[350];

  // PM2.5 конфигурация
  snprintf(configPayload, sizeof(configPayload),
    "{\"name\":\"PM2.5\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"ug/m3\",\"device_class\":\"pm25\",\"unique_id\":\"%s_pm25\"}",
    MQTT_TOPIC_PM25, DEVICE_ID);
  mqtt.publish(MQTT_CONFIG_PM25, configPayload, true);

  // CO2 конфигурация
  snprintf(configPayload, sizeof(configPayload),
    "{\"name\":\"CO2\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"ppm\",\"device_class\":\"carbon_dioxide\",\"unique_id\":\"%s_co2\"}",
    MQTT_TOPIC_CO2, DEVICE_ID);
  mqtt.publish(MQTT_CONFIG_CO2, configPayload, true);

  // Температура конфигурация
  snprintf(configPayload, sizeof(configPayload),
    "{\"name\":\"Temperature\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"C\",\"device_class\":\"temperature\",\"unique_id\":\"%s_temp\"}",
    MQTT_TOPIC_TEMP, DEVICE_ID);
  mqtt.publish(MQTT_CONFIG_TEMP, configPayload, true);

  // Влажность конфигурация
  snprintf(configPayload, sizeof(configPayload),
    "{\"name\":\"Humidity\",\"state_topic\":\"%s\",\"unit_of_measurement\":\"%%\",\"device_class\":\"humidity\",\"unique_id\":\"%s_humidity\"}",
    MQTT_TOPIC_HUMIDITY, DEVICE_ID);
  mqtt.publish(MQTT_CONFIG_HUMIDITY, configPayload, true);

  Serial.println("Home Assistant auto-discovery config sent");
}

// Публикация данных в MQTT
void publishSensorData(float pm25, int co2, float temp, float humidity) {
  if (!mqtt.connected()) {
    connectMQTT();
  }

  if (mqtt.connected()) {
    char payload[16];

    if (pm25 >= 0) {
      snprintf(payload, sizeof(payload), "%.1f", pm25);
      mqtt.publish(MQTT_TOPIC_PM25, payload);
    }

    if (co2 >= 0) {
      snprintf(payload, sizeof(payload), "%d", co2);
      mqtt.publish(MQTT_TOPIC_CO2, payload);
    }

    if (temp >= 0) {
      snprintf(payload, sizeof(payload), "%.1f", temp);
      mqtt.publish(MQTT_TOPIC_TEMP, payload);
    }

    if (humidity >= 0) {
      snprintf(payload, sizeof(payload), "%.1f", humidity);
      mqtt.publish(MQTT_TOPIC_HUMIDITY, payload);
    }

    Serial.println("MQTT: data published");
  }

  mqtt.loop();  // обработка MQTT
}

void setup() {
  Serial.begin(115200);             // запускаем Serial-порт на скорости 115200 бод
  while (!Serial) {                 // ждём пока Serial-порт не будет готов
    delay(100);                     // пауза 100мс между проверками
  }

  Wire.begin();                     // запускаем I2C шину
  Wire.setWireTimeout(25000);       // таймаут I2C 25мс (стандартный 1мс слишком мал для SPS30)

  sps.begin(Wire, SPS30_I2C_ADDR_69);  // подключаемся к SPS30 по I2C с адресом 0x69

  sps.deviceReset();                // перезагружаем SPS30 для чистого старта
  delay(100);                       // ждём 100мс пока датчик перезагрузится

  int8_t serialNumber[32] = {0};    // буфер для серийного номера датчика
  error = sps.readSerialNumber(serialNumber, 32);  // читаем серийный номер SPS30
  if (error != NO_ERROR) {          // если ошибка при чтении —
    Serial.print("SPS30 error reading serial: ");
    errorToString(error, errorMessage, sizeof errorMessage);  // преобразуем код ошибки в текст
    Serial.println(errorMessage);   // выводим текст ошибки
  } else {
    Serial.print("SPS30 serial: ");
    Serial.println((const char*)serialNumber);  // выводим серийный номер
  }

  uint8_t fwMajor, fwMinor;        // переменные для версии прошивки (мажор.минор)
  error = sps.readFirmwareVersion(fwMajor, fwMinor);  // читаем версию прошивки SPS30
  if (error == NO_ERROR) {          // если успешно —
    Serial.print("SPS30 firmware: ");
    Serial.print(fwMajor);         // выводим мажорную версию
    Serial.print(".");
    Serial.println(fwMinor);       // выводим минорную версию
  } else {
    Serial.print("SPS30 error reading firmware version: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }

  error = sps.writeAutoCleaningInterval(auto_clean_interval);  // устанавливаем интервал автоочистки вентилятора
  if (error != NO_ERROR) {
    Serial.print("SPS30 error setting auto-clean interval: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }

  // стартовая продувка вентилятора для очистки от накопившейся пыли
  // fan cleaning работает только в режиме измерения, поэтому сначала запускаем измерение
  error = sps.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
  if (error != NO_ERROR) {
    Serial.print("SPS30 error starting measurement for cleaning: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  } else {
    delay(1000);  // даём вентилятору раскрутиться
    error = sps.startFanCleaning();
    if (error != NO_ERROR) {
      Serial.print("SPS30 error starting fan cleaning: ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
    } else {
      Serial.println("SPS30 fan cleaning started");
      delay(10000);  // ждём 10 секунд пока очистка завершится
    }
    sps.stopMeasurement();  // останавливаем измерение после очистки
  }

  if (airSensor.begin() == false) {  // инициализируем SCD30; если не найден —
    Serial.println("SCD30 not detected. Please check wiring. Freezing...");
    while (1)                        // зависаем навсегда (дальше работать без датчика нет смысла)
      ;
  }
  Serial.println("SCD30 detected");  // SCD30 найден и готов к работе

  // Подключение к WiFi и MQTT
  connectWiFi();
  connectMQTT();

  Serial.println("DATA_START");      // маркер для Python-скрипта: дальше пойдут данные
  Serial.println("pm25,co2,temp,humidity");  // заголовок CSV-таблицы

  delay(2000);                       // ждём 2 секунды перед первым измерением
}

void loop() {
  delay(5000);
  // запускаем вентилятор SPS30
  error = sps.startMeasurement(SPS30_OUTPUT_FORMAT_OUTPUT_FORMAT_FLOAT);
  if (error != NO_ERROR) {
    Serial.print("# SPS30 error starting measurement: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }

  delay(5000);                      // ждём 5 секунд пока вентилятор раскрутится и соберёт данные

  float pm25 = -1;                   // PM2.5 (пыль), -1 = данных пока нет
  int co2 = -1;                      // CO2 в ppm, -1 = данных пока нет
  float temp = -1, humidity = -1;    // температура и влажность, -1 = данных пока нет

  uint16_t dataReadyFlag = 0;        // флаг: есть ли готовые данные у SPS30
  error = sps.readDataReadyFlag(dataReadyFlag);  // спрашиваем SPS30, готовы ли данные
  if (error != NO_ERROR) {           // ошибка при запросе —
    Serial.print("# SPS30 error reading data-ready flag: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  } else if (dataReadyFlag) {        // данные готовы —
    float mc1p0 = 0, mc2p5 = 0, mc4p0 = 0, mc10p0 = 0;      // массовые концентрации частиц (µg/m³)
    float nc0p5 = 0, nc1p0 = 0, nc2p5 = 0, nc4p0 = 0, nc10p0 = 0;  // числовые концентрации (#/cm³)
    float typicalParticleSize = 0;   // типичный размер частицы (мкм)

    error = sps.readMeasurementValuesFloat(mc1p0, mc2p5, mc4p0, mc10p0,  // читаем все значения с SPS30
                                            nc0p5, nc1p0, nc2p5, nc4p0,
                                            nc10p0, typicalParticleSize);
    if (error != NO_ERROR) {         // ошибка при чтении измерений —
      Serial.print("# SPS30 error reading measurement: ");
      errorToString(error, errorMessage, sizeof errorMessage);
      Serial.println(errorMessage);
    } else {
      pm25 = mc2p5;                  // сохраняем значение PM2.5 для вывода
    }
  } else {
    Serial.println("# SPS30 data not ready");  // данные ещё не готовы, попробуем в следующем цикле
  }

  // останавливаем вентилятор SPS30 для экономии ресурса
  error = sps.stopMeasurement();
  if (error != NO_ERROR) {
    Serial.print("# SPS30 error stopping measurement: ");
    errorToString(error, errorMessage, sizeof errorMessage);
    Serial.println(errorMessage);
  }

  if (airSensor.dataAvailable()) {   // проверяем, есть ли данные у SCD30
    co2 = airSensor.getCO2();        // читаем CO2 (ppm)
    temp = airSensor.getTemperature();    // читаем температуру (°C)
    humidity = airSensor.getHumidity();   // читаем влажность (%)
  } else {
    Serial.println("# SCD30 waiting for data");  // SCD30 ещё не готов
  }

  if (pm25 >= 0 || co2 >= 0) {      // если хотя бы один датчик дал данные — выводим CSV строку
    if (pm25 >= 0) Serial.print(pm25);    // PM2.5 (пусто если нет данных)
    Serial.print(",");                     // разделитель
    if (co2 >= 0) Serial.print(co2);      // CO2 (пусто если нет данных)
    Serial.print(",");
    if (temp >= 0) Serial.print(temp, 1);      // температура с 1 знаком после запятой
    Serial.print(",");
    if (humidity >= 0) Serial.print(humidity, 1);  // влажность с 1 знаком после запятой
    Serial.println();                      // конец строки

    // Отправляем данные в Home Assistant через MQTT
    publishSensorData(pm25, co2, temp, humidity);
  }
}
