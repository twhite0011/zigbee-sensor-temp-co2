# zigbee-sensor-temp-co2

Minimal ESP-IDF Zigbee end-device for **Seeed XIAO ESP32-C6** + **SHTC3** + **SCD4X**.

## Features
- Zigbee end-device (ZED) for Zigbee2MQTT/Home Assistant
- Reports:
  - Temperature (cluster `0x0402`)
  - Humidity (cluster `0x0405`)
  - Carbon dioxide (cluster `0x040D`, when CO2 sensor is enabled)
- Sensor update interval: **30 seconds** (publish cadence follows coordinator reporting config)
- Includes external Zigbee2MQTT converter: `z2m_converter.js`

## Wiring (I2C)
- XIAO GPIO22 -> SHTC3 SDA + SCD4X SDA
- XIAO GPIO23 -> SHTC3 SCL + SCD4X SCL
- XIAO 3.3V -> SHTC3 VCC + SCD4X VCC
- XIAO GND -> SHTC3 GND + SCD4X GND

## Build and flash
Prerequisites:
- ESP-IDF `v5.5.x` installed locally
- Serial port access to `/dev/ttyACM0` (on Linux, user in `dialout`/`uucp`)

```bash
git clone https://github.com/twhite0011/zigbee-sensor-temp-co2.git
cd zigbee-sensor-temp-co2
source ~/esp/esp-idf/export.sh
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

If monitor fails in a non-interactive shell, run:

```bash
source ~/esp/esp-idf/export.sh
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```

## Enable or disable CO2 sensor
CO2 support is controlled by `CONFIG_APP_ENABLE_CO2_SENSOR`.

- Default in this repository: **enabled** (`CONFIG_APP_ENABLE_CO2_SENSOR=y` in `sdkconfig`)
- To disable (temperature/humidity-only hardware):
  1. Run `idf.py menuconfig`
  2. Go to `Application Config`
  3. Uncheck `Enable SCD4X CO2 sensor`
  4. Rebuild and flash

When CO2 is disabled, the Zigbee model changes to `XIAO-SHTC3` and CO2 cluster/entity is not exposed.

## Zigbee2MQTT setup and pairing
1. Configure Zigbee2MQTT to load `z2m_converter.js` as an external converter in `configuration.yaml`:
   ```yaml
   external_converters:
     - /path/to/zigbee-sensor-temp-co2/z2m_converter.js
   ```
2. Restart Zigbee2MQTT.
3. Enable `permit_join` in Zigbee2MQTT.
4. Flash/reboot device.
5. Wait for interview; device should expose temperature, humidity, and CO2 entities.
6. If you change converter reporting settings later, run Zigbee2MQTT **Reconfigure** for this device (or re-pair) so new reporting is applied.

Expected runtime logs after join:
- `zigbee: Rejoined network PAN=... CH=... SHORT=...` (or joined network log)
- `sensors: SHTC3 initialized on I2C 0x70`
- `sensors: SCD4X initialized on I2C 0x62`
- `zigbee: Updated temp=<temp> C humidity=<humidity> % co2=<ppm> ppm`

## Notes
- Firmware scans all Zigbee channels allowed by `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` when joining a new network.
- After changing the coordinator Zigbee channel or moving the device to a different network, run `idf.py -p /dev/ttyACM0 erase-flash flash` before re-pairing so saved Zigbee state does not force a rejoin on the old channel.
- SCD4X runs periodic measurement mode and publishes latest ppm value each report cycle.
- Current external converter default reporting:
  - Temperature: min 40 s, max 60 s, change 1.0 C (`100`)
  - Humidity: min 40 s, max 60 s, change 1.0 %RH (`100`)
  - CO2: min 40 s, max 60 s, change 5 ppm (`0.000005` in raw cluster units)
- If pairing fails, run `idf.py -p /dev/ttyACM0 erase-flash flash`.

## License
This project source is licensed under the MIT License. See `LICENSE`.

## Third-party components
`espressif/esp-zigbee-lib` and `espressif/esp-zboss-lib` are resolved by ESP-IDF Component Manager from `main/idf_component.yml` and are not required to be committed in this repository. They keep their own upstream licenses.
