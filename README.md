# zigbee-sensor-temp-co2

Minimal ESP-IDF Zigbee end-device for **Seeed XIAO ESP32-C6** + **SHTC3** + **SCD4X**.

## Features
- Zigbee end-device (ZED) for Zigbee2MQTT/Home Assistant
- Reports:
  - Temperature (cluster `0x0402`)
  - Humidity (cluster `0x0405`)
  - Carbon dioxide (cluster `0x040D`)
- Reporting interval: **30 seconds**
- Includes external Zigbee2MQTT converter: `z2m_converter.js`

## Wiring (I2C)
- XIAO GPIO22 -> SHTC3 SDA + SCD4X SDA
- XIAO GPIO23 -> SHTC3 SCL + SCD4X SCL
- XIAO 3.3V -> SHTC3 VCC + SCD4X VCC
- XIAO GND -> SHTC3 GND + SCD4X GND

## Build and flash
```bash
cd /home/nonya/gits/zigbee-sensor-temp-co2
source /home/nonya/gits/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Zigbee2MQTT setup and pairing
1. Configure Zigbee2MQTT to load `z2m_converter.js` as an external converter in `configuration.yaml`:
   ```yaml
   external_converters:
     - /path/to/zigbee-sensor-temp-co2/z2m_converter.js
   ```
2. Restart Zigbee2MQTT.
3. Enable `permit_join` in Zigbee2MQTT.
4. Flash/reboot device.
5. Wait for interview; device should expose temperature, humidity, and co2 entities.

## Notes
- Firmware currently uses Zigbee channel **11** only.
- SCD4X runs periodic measurement mode and publishes latest ppm value each report cycle.
- If pairing fails, run `idf.py -p /dev/ttyACM0 erase-flash flash`.
