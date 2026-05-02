#include <WiFi.h>
#include <Wire.h>
#include "time.h"

// -------- WIFI --------
const char* ssid     = "TTUguest";
const char* password = "fearthestache";  // TTUguest has no password

// -------- NTP SERVER --------
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;  // CST = UTC-6
const int   daylightOffset_sec = 3600;

// -------- DS3231 --------
#define DS3231_ADDR 0x68

byte dec_to_bcd(byte val) {
  return ((val / 10) * 16) + (val % 10);
}

void setRTC(int yr, int mon, int day, int hr, int mn, int sec) {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);  // start at seconds register
  Wire.write(dec_to_bcd(sec));
  Wire.write(dec_to_bcd(mn));
  Wire.write(dec_to_bcd(hr));
  Wire.write(dec_to_bcd(1));  // day of week (not used)
  Wire.write(dec_to_bcd(day));
  Wire.write(dec_to_bcd(mon));
  Wire.write(dec_to_bcd(yr - 2000));
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(1000);

  Serial.println("\n=== ESP32 RTC Time Sync ===");

  // Connect to WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int timeout = 20;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed!");
    Serial.println("RTC NOT updated");
    return;
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Get time from NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time from NTP");
    return;
  }

  Serial.println("Got time from internet:");
  Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");

  // Write to RTC
  setRTC(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  Serial.println("RTC updated successfully!");
  Serial.printf("Set to: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900,
                timeinfo.tm_mon + 1,
                timeinfo.tm_mday,
                timeinfo.tm_hour,
                timeinfo.tm_min,
                timeinfo.tm_sec);

  WiFi.disconnect(true);
  Serial.println("\nDisconnected from WiFi");
  Serial.println("You can now move RTC to RPi");
}

void loop() {
  // Nothing
}
