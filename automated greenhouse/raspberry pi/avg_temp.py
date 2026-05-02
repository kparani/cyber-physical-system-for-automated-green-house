import paho.mqtt.client as mqtt
import csv
import os
import threading
from datetime import datetime
import smbus2

# -------- RTC (DS3231) --------
bus = smbus2.SMBus(1)
DS3231_ADDR = 0x68

def bcd_to_dec(val):
    return (val // 16) * 10 + (val % 16)

def get_rtc_time():
    data = bus.read_i2c_block_data(DS3231_ADDR, 0x00, 7)
    sec = bcd_to_dec(data[0] & 0x7F)
    mn  = bcd_to_dec(data[1])
    hr  = bcd_to_dec(data[2] & 0x3F)
    day = bcd_to_dec(data[4])
    mon = bcd_to_dec(data[5] & 0x1F)
    yr  = bcd_to_dec(data[6]) + 2000
    return f"{yr}-{mon:02d}-{day:02d} {hr:02d}:{mn:02d}:{sec:02d}"

# -------- CONFIG --------
BASE_DIR = "/home/tent1/tent_data"
GROUPED_DIR = os.path.join(BASE_DIR, "grouped_data")

# Individual logs
TEMP_LOG = os.path.join(BASE_DIR, "temperature_log.csv")
CO2_LOG = os.path.join(BASE_DIR, "co2_log.csv")
LIGHT_LOG = os.path.join(BASE_DIR, "light_log.csv")
BATT_LOG = os.path.join(BASE_DIR, "battery_log.csv")
RPI_TEMP_LOG = os.path.join(BASE_DIR, "rpi_temperature_log.csv")

# -------- STATE --------
state = {}

# -------- CSV SETUP --------
def init_csv():
    os.makedirs(GROUPED_DIR, exist_ok=True)

    if not os.path.exists(TEMP_LOG):
        with open(TEMP_LOG, "w", newline="") as f:
            csv.writer(f).writerow(["timestamp", "device", "temperature"])
        print(f"[INFO] Created: {TEMP_LOG}")

    if not os.path.exists(CO2_LOG):
        with open(CO2_LOG, "w", newline="") as f:
            csv.writer(f).writerow(["timestamp", "device", "co2"])
        print(f"[INFO] Created: {CO2_LOG}")

    if not os.path.exists(LIGHT_LOG):
        with open(LIGHT_LOG, "w", newline="") as f:
            csv.writer(f).writerow(["timestamp", "device", "light_intensity"])
        print(f"[INFO] Created: {LIGHT_LOG}")

    if not os.path.exists(BATT_LOG):
        with open(BATT_LOG, "w", newline="") as f:
            csv.writer(f).writerow(["timestamp", "device", "battery"])
        print(f"[INFO] Created: {BATT_LOG}")

    if not os.path.exists(RPI_TEMP_LOG):
        with open(RPI_TEMP_LOG, "w", newline="") as f:
            csv.writer(f).writerow(["timestamp", "temperature"])
        print(f"[INFO] Created: {RPI_TEMP_LOG}")

    for tent in ["A", "B", "C", "D", "E"]:
        path = os.path.join(GROUPED_DIR, f"Tent_{tent}.csv")
        if not os.path.exists(path):
            with open(path, "w", newline="") as f:
                csv.writer(f).writerow([
                    "timestamp",
                    "avg_temp", "temp_devices",
                    "avg_co2", "co2_devices",
                    "avg_light", "light_devices"
                ])
            print(f"[INFO] Created: {path}")

# -------- RPi CPU TEMPERATURE --------
def get_rpi_temp():
    try:
        with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
            temp_millidegrees = int(f.read().strip())
        return temp_millidegrees / 1000.0
    except:
        return None

# -------- LOG GROUPED DATA (every 5 min) --------
def log_grouped():
    ts = get_rtc_time()
    now = datetime.now()
    STALE_MINUTES = 35

    rpi_temp = get_rpi_temp()
    if rpi_temp is not None:
        with open(RPI_TEMP_LOG, "a", newline="") as f:
            csv.writer(f).writerow([ts, f"{rpi_temp:.2f}"])
        print(f"[{ts}] RPi CPU: {rpi_temp:.2f}°C")

    for tent in ["A", "B", "C", "D", "E"]:
        temp_devices = {k: v for k, v in state.items() if k.startswith(f"{tent}T") and "temperature" in v}
        valid_temps = []
        valid_temp_names = []

        for device, data in temp_devices.items():
            val = data.get("temperature")
            last_seen = data.get("last_seen")

            if val is None or last_seen is None:
                continue

            minutes_ago = (now - last_seen).total_seconds() / 60
            if minutes_ago > STALE_MINUTES:
                print(f"[WARN] {device} stale ({minutes_ago:.0f} min) — excluded from Tent_{tent}")
                continue

            valid_temps.append(val)
            valid_temp_names.append(device)

        avg_temp = sum(valid_temps) / len(valid_temps) if valid_temps else None
        temp_str = f"{avg_temp:.2f}" if avg_temp else ""
        temp_devices_str = ",".join(valid_temp_names) if valid_temp_names else ""

        co2_devices = {k: v for k, v in state.items() if k.startswith(f"{tent}C") and "co2" in v}
        valid_co2 = []
        valid_co2_names = []

        for device, data in co2_devices.items():
            val = data.get("co2")
            last_seen = data.get("last_seen")

            if val is None or last_seen is None:
                continue

            minutes_ago = (now - last_seen).total_seconds() / 60
            if minutes_ago > STALE_MINUTES:
                continue

            valid_co2.append(val)
            valid_co2_names.append(device)

        avg_co2 = sum(valid_co2) / len(valid_co2) if valid_co2 else None
        co2_str = f"{avg_co2:.0f}" if avg_co2 else ""
        co2_devices_str = ",".join(valid_co2_names) if valid_co2_names else ""

        light_devices = {k: v for k, v in state.items() if k.startswith(f"{tent}L") and "light" in v}
        valid_light = []
        valid_light_names = []

        for device, data in light_devices.items():
            val = data.get("light")
            last_seen = data.get("last_seen")

            if val is None or last_seen is None:
                continue

            minutes_ago = (now - last_seen).total_seconds() / 60
            if minutes_ago > STALE_MINUTES:
                continue

            valid_light.append(val)
            valid_light_names.append(device)

        avg_light = sum(valid_light) / len(valid_light) if valid_light else None
        light_str = f"{avg_light:.2f}" if avg_light else ""
        light_devices_str = ",".join(valid_light_names) if valid_light_names else ""

        if avg_temp or avg_co2 or avg_light:
            path = os.path.join(GROUPED_DIR, f"Tent_{tent}.csv")
            with open(path, "a", newline="") as f:
                csv.writer(f).writerow([
                    ts,
                    temp_str, temp_devices_str,
                    co2_str, co2_devices_str,
                    light_str, light_devices_str
                ])
            print(f"[{ts}] Tent_{tent} — T:{temp_str} C:{co2_str} L:{light_str}")

    threading.Timer(300, log_grouped).start()

# -------- MQTT CALLBACKS --------
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe("esp32/#")
        print("[MQTT] Connected and subscribed to esp32/#")

def on_message(client, userdata, msg):
    parts = msg.topic.split("/")
    if len(parts) < 3:
        return

    full_device = parts[1]
    kind = parts[2]
    device = full_device.replace("esp32_", "")
    value = msg.payload.decode()
    ts = get_rtc_time()

    print(f"[{ts}] Topic: {msg.topic} | Payload: {value}")

    try:
        fval = float(value)
    except ValueError:
        print(f"[WARN] Bad payload: {value}")
        return

    if device not in state:
        state[device] = {}

    if kind == "temperature":
        with open(TEMP_LOG, "a", newline="") as f:
            csv.writer(f).writerow([ts, device, value])
        state[device]["temperature"] = fval
        state[device]["last_seen"] = datetime.now()

    elif kind == "co2":
        with open(CO2_LOG, "a", newline="") as f:
            csv.writer(f).writerow([ts, device, value])
        state[device]["co2"] = fval
        state[device]["last_seen"] = datetime.now()

    elif kind == "light":
        with open(LIGHT_LOG, "a", newline="") as f:
            csv.writer(f).writerow([ts, device, value])
        state[device]["light"] = fval
        state[device]["last_seen"] = datetime.now()

    elif kind == "battery":
        with open(BATT_LOG, "a", newline="") as f:
            csv.writer(f).writerow([ts, device, value])

# -------- MAIN --------
init_csv()

threading.Timer(300, log_grouped).start()
print("[INFO] Grouped logger runs every 5 minutes")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("localhost", 1883, 60)

print("[INFO] Logger running. Press Ctrl+C to stop.")
client.loop_forever()
