#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// -------- WIFI CREDENTIALS --------
const char* ssid     = "TP-Link_AP_1AAA";
const char* password = "12608201";

// -------- MQTT BROKER --------
const char* mqtt_server  = "192.168.0.103";
const int   mqtt_port    = 1883;
const char* mqtt_topic   = "esp32/esp32_AT3/temperature";
const char* mqtt_batt    = "esp32/esp32_AT3/battery";
const char* mqtt_id      = "esp32_AT3";

// -------- DEEP SLEEP INTERVAL -------
#define SLEEP_MINUTES        5
#define uS_TO_MIN            60000000ULL
#define SLEEP_US             (SLEEP_MINUTES * uS_TO_MIN)

// -------- BATTERY SCHEDULE --------
#define BATTERY_EVERY_N_WAKES  72      // 72 X 5 min = 6hrs so battery value get published every 6 hours

// -------- DS18B20 --------
#define ONE_WIRE_BUS 23
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// -------- BATTERY ADC --------
#define BATT_PIN       34
#define BATT_MIN_V     3.0f    // 18650 empty voltage
#define BATT_MAX_V     4.2f    // 18650 full voltage
#define ADC_MAX        4095.0f
#define VREF           3.3f
#define DIVIDER_RATIO  2.0f    // R1=R2=10k, so Vout = Vin/2

// -------- RTC MEMORY (survives deep sleep) --------
RTC_DATA_ATTR int wakeCount = 0;

// -------- MQTT & WiFi --------
WiFiClient   espClient;
PubSubClient client(espClient);

// -------- FORWARD DECLARATION --------
void goToSleep();

// -------- WIFI CONNECT --------
bool connectWiFi(unsigned long timeoutMs = 10000) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("WiFi timeout — going back to sleep");
      return false;
    }
    delay(200);
  }
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

// -------- MQTT CONNECT --------
bool connectMQTT(unsigned long timeoutMs = 5000) {
  client.setServer(mqtt_server, mqtt_port);

  unsigned long start = millis();
  while (!client.connected()) {
    if (millis() - start > timeoutMs) {
      Serial.println("MQTT timeout — going back to sleep");
      return false;
    }
    if (client.connect(mqtt_id)) {
      Serial.println("MQTT connected");
    } else {
      delay(500);
    }
  }
  return true;
}

// -------- READ BATTERY % --------
float readBatteryPercent() {
  // Take 10 samples and average to reduce ADC noise
  int raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(BATT_PIN);
    delay(10);
  }
  raw /= 10;

  // Convert ADC reading to actual battery voltage
  float voltage = (raw / ADC_MAX) * VREF * DIVIDER_RATIO;

  // Map voltage to percentage
  float percent = ((voltage - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V)) * 100.0f;

  // Clamp between 0 and 100
  if (percent > 100.0f) percent = 100.0f;
  if (percent < 0.0f)   percent = 0.0f;

  Serial.printf("Battery ADC raw: %d | Voltage: %.2fV | Level: %.1f%%\n", raw, voltage, percent);
  return percent;
}

// -------- SETUP (runs once per wake) --------
void setup() {
  Serial.begin(115200);
  delay(500);

  wakeCount++;
  Serial.printf("\n--- ESP32_1 woke up | Wake #%d ---\n", wakeCount);

  // 1. Init sensor
  sensors.begin();

  // 2. Connect WiFi
  if (!connectWiFi()) goToSleep();

  // 3. Connect MQTT
  if (!connectMQTT()) goToSleep();

  // 4. Read & publish temperature
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor error — going back to sleep");
    goToSleep();
  }

  char tempStr[8];
  dtostrf(tempC, 4, 2, tempStr);
  client.publish(mqtt_topic, tempStr, true);
  Serial.printf("Published %.2f C to %s\n", tempC, mqtt_topic);

  // 5. Read & publish battery every N wakes
  if (wakeCount % BATTERY_EVERY_N_WAKES == 0) {
    float battPct = readBatteryPercent();

    char battStr[8];
    dtostrf(battPct, 4, 1, battStr);
    client.publish(mqtt_batt, battStr, true);
    Serial.printf("Published %.1f%% to %s\n", battPct, mqtt_batt);
  } else {
    Serial.printf("Battery check in %d wake(s)\n",
                  BATTERY_EVERY_N_WAKES - (wakeCount % BATTERY_EVERY_N_WAKES));
  }

  // 6. Flush and sleep
  client.loop();
  delay(500);
  goToSleep();
}

void loop() {}

// -------- GO TO SLEEP --------
void goToSleep() {
  Serial.printf("Sleeping for %d minutes...\n", SLEEP_MINUTES);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup(SLEEP_US);
  esp_deep_sleep_start();
}