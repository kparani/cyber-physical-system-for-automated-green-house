#include <Wire.h>

// -------- DS3231 --------
#define DS3231_ADDR 0x68

byte bcd_to_dec(byte val) {
  return (val / 16 * 10) + (val % 16);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(500);
  Serial.println("\n=== RTC Time Reader ===");
}

void loop() {
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_ADDR, 7);

  byte sec = bcd_to_dec(Wire.read() & 0x7F);
  byte mn  = bcd_to_dec(Wire.read());
  byte hr  = bcd_to_dec(Wire.read() & 0x3F);
  Wire.read();  // day of week
  byte day = bcd_to_dec(Wire.read());
  byte mon = bcd_to_dec(Wire.read() & 0x1F);
  int  yr  = bcd_to_dec(Wire.read()) + 2000;

  Serial.printf("RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                yr, mon, day, hr, mn, sec);

  delay(2000);
}
