
# 40-Node ESP32/MQTT Distributed IoT System with Deep Sleep Optimization
**Autonomous multi-node environmental monitoring system deployed at Texas Tech University's Fiber and Biopolymer Research Institute**

![System Status](https://img.shields.io/badge/status-production-brightgreen) ![Nodes](https://img.shields.io/badge/nodes-40%2B-blue) ![Uptime](https://img.shields.io/badge/uptime-99%25%2B-success)

---

## Project Overview

Designed and deployed a distributed wireless sensor network and control network to monitor and contriol field based greenhouse for cotton phenotyping research. The system autonomously collects temperature, CO₂, light intensity, and battery health data from 40+ ESP32 nodes, aggregates telemetry via MQTT to a Raspberry Pi base station, and exports timestamped CSV datasets for scientific analysis. A seperate network of ESP32 controlls the opening\closing of the greenhouse side flaps.

**Key Achievements:** 
- **30+ day battery life per node** with deep sleep optimization while maintaining 5-minute data granularity
- **Zero-configuration scalability** — add/remove sensors without code changes; system auto-detects and aggregates
- **Fault-tolerant architecture** — gracefully handles edge node failures; continues operation with partial sensor arrays (e.g., tent with 2/5 sensors operational still reports valid spatial averages)
- **Flexible deployment density** — supports variable sensor counts per tent (1-N nodes) based on research requirements without infrastructure modifications

## Technical Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| **Edge Nodes** | ESP32 DevKit | Low-power wireless sensing |
| **Sensors** | DS18B20, SenseAir S8, VEML7700 | Temp, CO₂, Light |
| **Communication** | MQTT over WiFi (2.4GHz) | Publish-subscribe telemetry |
| **Message Broker** | Mosquitto (RPi) | Central data aggregation |
| **Data Logger** | Python 3 + smbus2 | Multi-threaded CSV logging |
| **Timekeeping** | DS3231 RTC | Persistent timestamps (survives power loss) |
| **Power** | 18650 Li-ion + deep sleep | 30+ day autonomy |

---

## Key Features

### 1. Distributed Sensor Network
- **40+ autonomous nodes** across 3 active tents (A-C) with infrastructure for 5 tents (A-E) to support future scaling
- **Dynamic device discovery** — system automatically detects and integrates new sensors without configuration changes; handles variable node counts per tent seamlessly
- **Hierarchical naming scheme** enables unique identification and logical grouping:
  - **First character (letter)** = Tent ID (A, B, C, D, E)
  - **Second character (letter)** = Parameter type (T=Temperature, C=CO₂, L=Light intensity)
  - **Third character (number)** = Unique sensor instance within that tent/type combination
  - Example: `AT1`, `AT2`, `AT3` = three temperature sensors in Tent A; `AC1`, `AC2` = two CO₂ sensors in Tent A
- **Infinite scalability within naming convention** — adding sensors requires only incrementing the instance number (e.g., AC6, AC7, BT12) with zero code changes; RPi logger auto-detects and aggregates
- **Per-tent spatial averaging** — system groups sensors by tent prefix (all A* devices → Tent_A.csv) regardless of sensor count, enabling flexible deployment density based on research needs, 

### 2. Low-Power Design
- **Deep sleep between readings** — ESP32 consumes ~10µA in sleep mode vs ~240mA active
- **Minimal wake time** — each cycle: WiFi connect (2-3s) + sensor read (1-2s) + MQTT publish (1s) = ~5 seconds active per wake
- **Configurable wake intervals** — 5 min (temp), 10 min (CO₂/light), 6 hr (battery)
- **Power budget per cycle:**
  - Active (5 sec): ~200mA average → 0.28mAh
  - Sleep (295 sec at 5-min interval): ~10µA → 0.0008mAh
  - **Effective average current: ~1.7mA** over 5-minute cycle
- **Battery life calculation:** 3500mAh ÷ 1.7mA ≈ **85 days theoretical**, 30+ days practical (accounting for battery sag and WiFi variance)
- **Wake counter in RTC memory** — persists across sleep cycles for battery reporting logic without incrementing during power loss

### 3. Robust Data Handling
- **Immediate persistence** — every MQTT message logged to individual CSVs (`temperature_log.csv`, `co2_log.csv`, `light_log.csv`, `battery_log.csv`) upon receipt, ensuring no data loss even during system crashes
- **In-memory state management** — RPi maintains latest value and `last_seen` timestamp per sensor in Python dictionary; grouped aggregation queries this state rather than re-parsing CSVs
- **Per-tent spatial averaging** — background thread calculates averages every 5 minutes from in-memory state, writes to grouped CSVs (`Tent_A.csv`, `Tent_B.csv`, etc.)
- **35-minute staleness detection** — sensors remain in averaging pool using their last-reported value for up to 35 minutes after dropout; automatically excluded from calculations beyond this threshold to prevent stale data dilution
- **Automatic recovery** — when previously-stale sensor resumes transmission, immediately re-included in next averaging cycle with no manual intervention required
- **Graceful degradation** — spatial averages calculated from available sensors only; Tent_A continues reporting even if AT3 fails, using AT1+AT2 average until AT3 recovers
- **Audit trail via device tracking** — grouped CSVs include `devices_used` column showing which sensors contributed to each average, enabling post-analysis fault detection

### 4. Production Reliability
- **Systemd service** — auto-starts on boot, restarts on failure
- **Hardware RTC timestamping** — accurate logging even without internet/NTP, the DS3231 RTC module keeps the clock running with backup battery.
- **Dual CSV output** — raw sensor data + grouped tent averages
- **Zero-touch operation** — runs headless for months

--


## Hardware Bill of Materials

**Per Temperature Node:**
- ESP32 DevKit (1x)
- DS18B20 waterproof temperature probe (1x)
- 18650 Li-ion battery 3500mAh (1x)
- 18650 battery holder (1x) — no built-in protection
- **Power regulation circuit:**
  - MCP1700-3302E/TO 3.3V LDO voltage regulator (1x)
  - 100µF electrolytic capacitor, 25V (1x) — regulator input filtering
  - 10µF ceramic capacitor, 50V (1x) — regulator output filtering
- 4.7kΩ pull-up resistor (1x) — for DS18B20 data line
- 10kΩ resistor (2x) — voltage divider for battery monitoring

**Per CO₂ Node:**
- ESP32 DevKit (1x)
- SenseAir S8 LP CO₂ sensor (1x)
- **5V regulation circuit for CO₂ sensor:**
  - L7805 voltage regulator (1x)
  - 100µF electrolytic capacitor, 25V (1x) — L7805 input filtering
  - 10µF ceramic capacitor, 50V (1x) — L7805 output filtering
- 4xAA battery holder (1x)
- AA alkaline batteries (4x)
- **ESP32 power regulation circuit:**
  - MCP1700-3302E/TO 3.3V LDO voltage regulator (1x)
  - 100µF electrolytic capacitor, 25V (1x) — regulator input
  - 10µF ceramic capacitor, 50V (1x) — regulator output
- 10kΩ resistor (2x) — voltage divider for battery monitoring

**Per Light Node:**
- ESP32 DevKit (1x)
- Adafruit VEML7700 lux sensor breakout (1x)
- 18650 Li-ion battery 3500mAh (1x)
- 18650 battery holder (1x) — no built-in protection
- **Power regulation circuit:**
  - MCP1700-3302E/TO 3.3V LDO voltage regulator (1x)
  - 100µF electrolytic capacitor, 25V (1x) — regulator input filtering
  - 10µF ceramic capacitor, 50V (1x) — regulator output filtering
- 10kΩ resistor (2x) — voltage divider for battery monitoring

**Per Control Node:**
- ESP32 DevKit (1x)
- DS3231 RTC module with coin cell battery (1x)
- 4-channel relay module (1x)
- Power supply: mains power adapter 5V/2A

**Base Station:**
- Raspberry Pi 4 Model B, 4GB RAM (1x)
- DS3231 RTC module with CR2032 coin cell (1x)
- MicroSD card, 32GB Class 10 (1x)
- TP-Link WiFi access point (TL-WA801N or equivalent) (1x)
- Ethernet cable CAT5e (1x)
- 5V/3A USB-C power supply for RPi (1x)
- Raspberry Pi fan cooling system.

**Common Hardware:**
- Breadboard jumper wires
- Solder and soldering iron
- 3D-printed enclosures or ABS project boxes (optional but recommended)


## Software Architecture

### ESP32 Firmware (Arduino/C++)

**Core Loop:**
```
1. Wake from deep sleep
2. Connect to WiFi (10s timeout)
3. Connect to MQTT broker (5s timeout)
4. Read sensor(s) — average 3 readings for stability
5. Publish to topic: esp32/{device_id}/{sensor_type}
6. [Every N wakes] Read battery via ADC, publish to /battery
7. Disconnect WiFi
8. Enter deep sleep for M minutes
```

**Key Optimizations:**
- WiFi connection reuses credentials (no scan)
- MQTT `retain=true` so RPi gets last value on reconnect
- Voltage divider (2x 10kΩ) for battery monitoring on GPIO34
- Checksum validation on CO₂ UART reads

### Raspberry Pi Logger (Python)

**Architecture:**
```python
Main Thread:
  └─ MQTT subscriber (esp32/#)
      └─ on_message() → log to individual CSVs immediately
      
Background Timer (every 5 min):
  └─ log_grouped()
      ├─ Calculate per-tent averages (temp, CO₂, light)
      ├─ Exclude stale sensors (>35 min since last seen)
      ├─ Read RPi CPU temperature
      └─ Append to grouped CSVs
```

**Data Flow:**
```
MQTT message → Parse topic → Extract device ID & value type
             ↓
Individual CSV (temperature_log.csv, co2_log.csv, etc.)
             ↓
In-memory state dict (device → {value, last_seen})
             ↓
Every 5 min → Grouped CSV (Tent_A.csv, Tent_B.csv, etc.)
```

---

## Data Output Format

### Individual Logs
```csv
timestamp,          device, temperature
2026-04-19 10:00:15, AT1,   24.50
2026-04-19 10:00:17, AT2,   24.75
2026-04-19 10:00:19, AC1,   450
```

### Grouped Tent Averages
```csv
timestamp,          avg_temp, temp_devices,  avg_co2, co2_devices, avg_light, light_devices
2026-04-19 10:05:00, 24.62,   AT1,AT2,AT3,   450,     AC1,         1245.5,    AL1
2026-04-19 10:10:00, 24.58,   AT1,AT2,       452,     AC1,         1198.3,    AL1
```
*Note: AT3 excluded from second row due to staleness (>35 min)*

---

## Deployment Guide

### 1. Raspberry Pi Setup

**Install Mosquitto MQTT Broker:**
```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients -y
sudo systemctl enable mosquitto
```

**Configure for external connections:**
```bash
sudo nano /etc/mosquitto/mosquitto.conf
```
Add:
```
listener 1883
allow_anonymous true
```

**Set static IP (prevents DHCP drift):**
```bash
sudo nano /etc/network/interfaces
```
Add:
```
interface eth0
iface eth0 inet static
    address 192.168.0.103
    netmask 255.255.255.0
    gateway 192.168.0.254
```

**Install Python dependencies:**
```bash
pip3 install paho-mqtt smbus2 --break-system-packages
```

**Deploy logger script:**
```bash
mkdir -p /home/tent1/tent_data
# Copy avg_temp.py to /home/tent1/tent_data/
```

**Create systemd service:**
```bash
sudo nano /etc/systemd/system/templogger.service
```
```ini
[Unit]
Description=Tent Environmental Logger
After=network.target mosquitto.service

[Service]
Type=simple
User=tent1
WorkingDirectory=/home/tent1/tent_data
ExecStart=/usr/bin/python3 /home/tent1/tent_data/avg_temp.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

**Enable and start:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable templogger
sudo systemctl start templogger
```

### 2. DS3231 RTC Setup

**Wire to RPi:**
```
DS3231 VCC → RPi Pin 1 (3.3V)
DS3231 GND → RPi Pin 6 (GND)
DS3231 SDA → RPi Pin 3 (GPIO2)
DS3231 SCL → RPi Pin 5 (GPIO3)
```

**Set time (one-time via ESP32 with internet):**
- Flash `esp32_rtc_sync.ino` to ESP32
- Wire DS3231 to ESP32 (SDA→21, SCL→22)
- Connect to TTUguest WiFi → syncs NTP time to RTC
- Move RTC to RPi

### 3. ESP32 Node Deployment

**Configure each node:**
```cpp
const char* mqtt_server = "192.168.0.103";  // RPi IP
const char* mqtt_id     = "esp32_AT1";      // Unique per node
const char* mqtt_topic  = "esp32/esp32_AT1/temperature";
```

**Flash firmware via Arduino IDE:**
- Install ESP32 board support
- Install libraries: `PubSubClient`, `OneWire`, `DallasTemperature`, `Adafruit_VEML7700`
- Select Tools → Board → ESP32 Dev Module
- Upload

**Wiring Examples:**

*Temperature Node (DS18B20):*
```
DS18B20 VCC → ESP32 3.3V
DS18B20 GND → ESP32 GND
DS18B20 DAT → ESP32 GPIO23 + 4.7kΩ pull-up to 3.3V
Battery ADC → ESP32 GPIO34 (via voltage divider)
```

*CO₂ Node (SenseAir S8):*
```
4xAA → L7805 IN
L7805 OUT → S8 VCC
S8 GND → L7805 GND + ESP32 GND (shared)
S8 U-TX → ESP32 GPIO25
S8 U-RX → ESP32 GPIO26
```

---

## Challenges Overcome

### 1. IP Address Drift
**Problem:** RPi obtained different DHCP IP on each reboot, breaking all ESP32 connections.

**Solution:** Configured static IP on RPi via `/etc/network/interfaces`. All ESP32s hardcode `192.168.0.103` as broker address.

**Learning:** Static networking is critical for headless IoT deployments where DNS is unavailable.

---

### 2. Boot Interference on UART Pins
**Problem:** ESP32 failed to boot when CO₂ sensor TX wire connected to GPIO16 — `csum err:0x77!=0x51` on every startup.

**Solution:** Moved UART to GPIO25/26 which have no boot-time functions. Added 1kΩ series resistor on sensor TX as additional protection.

**Learning:** Always consult ESP32 strapping pin documentation before assigning peripherals.

---

### 3. Timekeeping Without Internet
**Problem:** System deployed in offline environment. RPi system clock resets to Jan 1970 on power loss, corrupting all timestamps.

**Solution:** Integrated DS3231 hardware RTC with coin cell backup. Python logger reads I2C time instead of `datetime.now()`. One-time NTP sync via ESP32 on university WiFi sets RTC permanently.

**Learning:** Battery-backed RTC is essential for data integrity in field deployments.

---

### 4. Sensor Dropout Handling
**Problem:** Occasional WiFi interference or battery depletion caused nodes to miss check-ins, skewing spatial averages.

**Solution:** Implemented 35-minute staleness detection. Each sensor stores `last_seen` timestamp. Grouped averaging excludes sensors that haven't reported in >35 min, preventing stale data from diluting live measurements.

**Learning:** Distributed systems need graceful degradation — partial data is better than no data.

---

### 5. Battery ADC Noise from Sensor
**Problem:** DS18B20 temperature conversion draws current spikes, causing 200mV ripple on 3.3V rail visible in battery ADC readings.

**Solution:** Added 100µF bulk capacitor across VCC/GND near ESP32. Moved battery ADC read to averaging loop (10 samples over 100ms).

**Learning:** Even low-power sensors create transient loads. Decoupling capacitors are non-negotiable in mixed-signal designs.

---

### 6. MH-Z19C CO₂ Sensor UART Issues
**Problem:** Initially attempted MH-Z19C sensor but encountered persistent UART corruption and boot failures even on safe GPIO pins.

**Solution:** Switched to SenseAir S8 which uses cleaner Modbus protocol and native 3.3V logic levels. S8 proved more stable with zero communication errors over 30+ day deployment.

**Learning:** Sensor selection matters — not all UART sensors handle sleep/wake cycles equally. Evaluate multiple options before committing to hardware.

---

## Performance Metrics

| Metric | Value | Notes |
|--------|-------|-------|
| **Battery Life** | 30+ days | 18650 3500mAh, 5-min wake cycle |
| **Data Granularity** | 5 minutes | Temperature nodes |
| **Network Uptime** | 99%+ | Measured over 60-day deployment |
| **Message Loss** | <0.1% | MQTT QoS 0 with retain flag |
| **Spatial Coverage** | 6 nodes/tent | Captures thermal gradients |
| **Time Accuracy** | ±2 seconds/month | DS3231 RTC drift spec |

---

## Future Improvements

### Hardware
- [ ] Solar panel integration for indefinite operation
- [ ] LoRa radio as WiFi backup (extend range, reduce power)
- [ ] Soil moisture sensors for irrigation feedback

### Software
- [ ] Web dashboard (Grafana/InfluxDB) for real-time visualization
- [ ] Alerting via email/SMS when sensors go offline or values exceed thresholds
- [ ] Over-the-air (OTA) firmware updates to deployed nodes
- [ ] Machine learning model for heat stress prediction from sensor fusion

### Scalability
- [ ] Multi-site deployment across TTU greenhouses
- [ ] Cloud integration (AWS IoT Core / Azure IoT Hub) for remote access
- [ ] Secure MQTT (TLS) for production deployments

---

## Lessons Learned

1. **Plan for failure from day one** — sensors die, batteries drain, WiFi drops. Design for degraded operation, not just happy path.

2. **Static IPs save headaches** — in embedded systems without DNS, DHCP is your enemy. Lock down addressing early.

3. **Hardware RTC is non-negotiable** — any system generating timestamped data needs independent timekeeping. System clocks are unreliable.

4. **Test thoroughly before deployment** — a week of bench testing catches 90% of field issues. Power cycling, network dropout simulation, and thermal stress testing are mandatory.

5. **Document as you build** — writing this README surfaced design decisions I'd forgotten. Future-you will thank present-you.

---

## Repository Structure

```
/cotton-heat-tent-monitoring/
├── README.md                    # This file
├── esp32/
│   ├── temperature_sensor/
│   │   └── esp32_temp.ino      # DS18B20 temperature node
│   ├── co2_sensor/
│   │   └── esp32_co2_uart.ino  # SenseAir S8 CO₂ node
│   ├── light_sensor/
│   │   └── esp32_light.ino     # VEML7700 light node
│   └── rtc_sync/
│       └── esp32_rtc_sync.ino  # One-time RTC time setter
├── raspberry_pi/
│   └── avg_temp.py             # MQTT logger with RTC support
├── docs/
│   ├── wiring_diagrams.md      # Detailed pin connections
│   └── troubleshooting.md      # Common issues and fixes
└── hardware/
    └── BOM.csv                  # Full bill of materials
```





