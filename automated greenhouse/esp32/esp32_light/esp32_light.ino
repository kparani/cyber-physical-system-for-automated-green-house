#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>

// -------- WIFI CREDENTIALS --------
const char* ssid     = "TP-Link_AP_1AAA";
const char* password = "12608201";

// -------- MQTT BROKER --------
const char* mqtt_server = "192.168.0.103";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "esp32/esp32_AL2/light";     // Change AL1 to your device
const char* mqtt_batt   = "esp32/esp32_AL2/battery";
const char* mqtt_id     = "esp32_AL2";

// -------- SLEEP --------
#define SLEEP_MINUTES  5
#define uS_TO_MIN      60000000ULL
#define SLEEP_US       (SLEEP_MINUTES * uS_TO_MIN)

// -------- BATTERY --------
#define BATT_PIN       34
#define BATT_MIN_V     3.0f
#define BATT_MAX_V     4.2f
#define BATTERY_EVERY_N_WAKES   // 36 x 10min = 6 hours

RTC_DATA_ATTR int wakeCount = 0;

// -------- VEML7700 --------
Adafruit_VEML7700 veml;

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
      Serial.println("WiFi timeout");
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
      Serial.println("MQTT timeout");
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

// -------- READ BATTERY --------
float readBatteryPercent() {
  int raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(BATT_PIN);
    delay(10);
  }
  raw /= 10;

  float voltage = (raw / 4095.0) * 3.3 * 2.0;  // voltage divider ratio 2.0
  float percent = ((voltage - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V)) * 100.0;
  
  if (percent > 100.0) percent = 100.0;
  if (percent < 0.0) percent = 0.0;
  
  return percent;
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(500);

  wakeCount++;
  Serial.printf("\n--- Light Sensor ESP32 woke up | Wake #%d ---\n", wakeCount);

  // Init VEML7700
  if (!veml.begin()) {
    Serial.println("VEML7700 not found — going to sleep");
    goToSleep();
  }
  Serial.println("VEML7700 found!");

  // Read light
  float lux = veml.readLux();
  Serial.printf("Light: %.2f lux\n", lux);

  // Connect WiFi
  if (!connectWiFi()) goToSleep();

  // Connect MQTT
  if (!connectMQTT()) goToSleep();

  // Publish light intensity
  char luxStr[10];
  dtostrf(lux, 6, 2, luxStr);
  bool published = client.publish(mqtt_topic, luxStr, true);
  if (published) {
    Serial.printf("Published %.2f lux to %s\n", lux, mqtt_topic);
  } else {
    Serial.println("Publish FAILED");
  }

  // Publish battery every 6 hours
  if (wakeCount % BATTERY_EVERY_N_WAKES == 0) {
    float battPct = readBatteryPercent();
    char battStr[8];
    dtostrf(battPct, 4, 1, battStr);
    client.publish(mqtt_batt, battStr, true);
    Serial.printf("Published battery: %.1f%%\n", battPct);
  } else {
    Serial.printf("Battery check in %d wake(s)\n", 
                  BATTERY_EVERY_N_WAKES - (wakeCount % BATTERY_EVERY_N_WAKES));
  }

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
