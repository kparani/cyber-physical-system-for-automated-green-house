#include <WiFi.h>
#include <PubSubClient.h>

// -------- WIFI --------
const char* ssid     = "TP-Link_AP_1AAA";
const char* password = "12608201";

// -------- MQTT --------
const char* mqtt_server = "192.168.0.103";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "esp32/esp32_AC1/co2";  // Change AC1 to your device
const char* mqtt_batt   = "esp32/esp32_AC1/battery";
const char* mqtt_id     = "esp32_AC1";

// -------- SLEEP --------
#define SLEEP_MINUTES  2
#define uS_TO_MIN      60000000ULL
#define SLEEP_US       (SLEEP_MINUTES * uS_TO_MIN)

// -------- BATTERY (if using) --------
#define BATT_PIN       34
#define BATT_MIN_V     3.0f
#define BATT_MAX_V     4.2f
#define BATTERY_EVERY_N_WAKES 72  // 72 x 10min = 12 hours

RTC_DATA_ATTR int wakeCount = 0;

// -------- SenseAir S8 UART --------
#define RX_PIN  25
#define TX_PIN  26
HardwareSerial s8Serial(2);

// -------- MQTT & WiFi --------
WiFiClient   espClient;
PubSubClient client(espClient);

void goToSleep();

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

int readCO2() {
  byte cmd[8] = {0xFE, 0x04, 0x00, 0x03, 0x00, 0x01, 0xD5, 0xC5};
  s8Serial.write(cmd, 8);
  delay(50);

  if (s8Serial.available() < 7) {
    Serial.println("S8 no response");
    return -1;
  }

  byte response[7];
  s8Serial.readBytes(response, 7);

  if (response[0] != 0xFE || response[1] != 0x04) {
    Serial.println("S8 bad response");
    return -1;
  }

  int co2ppm = (response[3] << 8) | response[4];
  return co2ppm;
}

float readBatteryPercent() {
  int raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(BATT_PIN);
    delay(10);
  }
  raw /= 10;

  float voltage = (raw / 4095.0) * 3.3 * 2.0;
  float percent = ((voltage - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V)) * 100.0;
  if (percent > 100.0) percent = 100.0;
  if (percent < 0.0) percent = 0.0;
  return percent;
}

void setup() {
  Serial.begin(115200);
  s8Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(500);

  wakeCount++;
  Serial.printf("\n--- CO2 ESP32 woke up | Wake #%d ---\n", wakeCount);

  if (!connectWiFi()) goToSleep();
  if (!connectMQTT()) goToSleep();

  // Read CO2 - take 3 readings
  int total = 0;
  int valid = 0;
  for (int i = 0; i < 3; i++) {
    int ppm = readCO2();
    if (ppm > 0) {
      total += ppm;
      valid++;
      Serial.printf("Reading %d: %d ppm\n", i + 1, ppm);
    }
    delay(2000);
  }

  if (valid == 0) {
    Serial.println("All CO2 readings failed");
    goToSleep();
  }

  int avgPPM = total / valid;
  Serial.printf("Average CO2: %d ppm\n", avgPPM);

  char ppmStr[8];
  itoa(avgPPM, ppmStr, 10);
  client.publish(mqtt_topic, ppmStr, true);
  Serial.printf("Published %d ppm\n", avgPPM);

  // Battery every 12 hours
  if (wakeCount % BATTERY_EVERY_N_WAKES == 0) {
    float battPct = readBatteryPercent();
    char battStr[8];
    dtostrf(battPct, 4, 1, battStr);
    client.publish(mqtt_batt, battStr, true);
    Serial.printf("Published battery: %.1f%%\n", battPct);
  }

  client.loop();
  delay(500);
  goToSleep();
}

void loop() {}

void goToSleep() {
  Serial.printf("Sleeping for %d minutes...\n", SLEEP_MINUTES);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup(SLEEP_US);
  esp_deep_sleep_start();
}
