# Smart Indoor IAQ Monitor

**Week 1 Task:** System Requirements Analysis, Block Diagram Design, and Architectural Scaffolding
**Status:** Educational reference implementation; hardware and safety review required before deployment

## Project Overview

This repository contains an ESP32-WROOM-32 reference architecture for an IoT-connected indoor air-quality monitor. The firmware polls an SCD30 CO2 sensor, BME680 environmental/gas sensor, and PMS5003 particulate sensor; evaluates a local CO2 alarm; and publishes the latest readings over MQTT/TLS when Wi-Fi is available. Acquisition and networking run in separate FreeRTOS tasks, with a one-entry overwrite queue so cloud latency cannot block sampling.

The system is an informational monitor, not a certified CO2 alarm, smoke detector, medical device, or life-safety instrument. Thresholds, calibration, deployment environment, enclosure, and regulatory requirements must be validated for the intended product.

## Repository Layout

```text
.
├── README.md
├── .gitignore
├── firmware/
│   ├── platformio.ini
│   ├── include/
│   │   ├── iaq_sensors.h
│   │   ├── network_service.h
│   │   ├── pms5003.h
│   │   ├── project_config.h
│   │   ├── secrets.example.h
│   │   └── telemetry.h
│   └── src/
│       ├── iaq_sensors.cpp
│       ├── main.cpp
│       ├── network_service.cpp
│       └── pms5003.cpp
└── simulation/
	 └── power_regulator.cir
```

## Preliminary BOM

| Qty | Item | Design role / selection note |
|---:|---|---|
| 1 | ESP32-WROOM-32 development board | ESP32 dual-core MCU with 2.4 GHz Wi-Fi and BLE; confirm the exact module variant and board pinout. |
| 1 | Sensirion SCD30 module | NDIR CO2, temperature, and RH; use I2C address 0x61 and check the breakout's logic/power requirements. |
| 1 | Bosch BME680 module | Temperature, RH, pressure, and gas resistance over I2C (0x76 or 0x77). Gas resistance alone is not a TVOC reading. |
| 1 | Plantower PMS5003 module | PM1.0/PM2.5/PM10 over 9600 8N1 UART; requires a 5 V rail for its fan. |
| 1 | 0.96-inch SSD1306-compatible OLED (optional) | I2C local readout; not driven by this initial firmware scaffold. |
| 1 | Alarm LED plus series resistor | GPIO-driven local visual indication. Select resistor for LED forward voltage and safe GPIO current. |
| 1 | Piezo buzzer plus transistor/MOSFET driver | GPIO-controlled audible indication; do not drive a higher-current buzzer directly from the MCU. |
| 1 | USB-C 5 V input assembly | USB-C receptacle, correct sink CC pull-downs, input protection, and current-rated connector/cable. No USB-PD negotiation is assumed. |
| 1 | Single-cell protected LiPo, 3.7 V nominal, 2000 mAh target | Backup energy source. Select cell protection, connector, temperature range, and charging limits from the cell datasheet. |
| 1 | Single-cell charger with power-path/load sharing (preferred) | Use a charger designed to power the system while charging. A TP4056 alone is not a power-path controller. |
| 1 | Regulated 5 V and 3.3 V conversion stages | Size for PMS5003 fan and ESP32 radio peaks. A 3.3 V LDO on a 3.7 V cell will drop out before the cell is fully discharged. |
| As req. | Decoupling, ESD, current-limit, connector, and passives | Final values and footprints are schematic/layout deliverables, not fixed by this logical scaffold. |

The BOM is preliminary, not a purchasable production BOM. Select exact modules and regulators only after checking datasheets, peak loads, charge profile, thermal limits, lifecycle, and certifications.

## Architecture and Wiring Overview

```text
USB-C 5 V ── protection ──┬── power-path / source mux ── 5 V rail ── PMS5003
									│                             └──────────── WS2812B* / loads
									└── single-cell charger ── LiPo backup
																		  │
												  source selection ─┴─ regulator(s) ── 3.3 V
																								  ├─ ESP32
																								  ├─ SCD30 + BME680 (I2C)
																								  └─ OLED (optional I2C)

SCD30 SDA/SCL ─┐
BME680 SDA/SCL ├── shared I2C ── ESP32 GPIO21 SDA / GPIO22 SCL
OLED SDA/SCL* ─┘
PMS5003 TX ──────────────────── ESP32 UART2 RX GPIO16
PMS5003 RX ──────────────────── ESP32 UART2 TX GPIO17
Alarm LED ───────────────────── ESP32 GPIO26 (with series resistor)
Buzzer driver input ─────────── ESP32 GPIO27
All logic grounds ───────────── common ground at the low-voltage system reference
```

`*` The firmware scaffold does not yet drive the OLED or WS2812B. Keep high-current LED/fan return currents away from sensor ground paths. Verify voltage domains, UART logic levels, I2C pull-up voltage and total bus capacitance, address straps, pin boot-strapping constraints, and connector pinout against the exact breakout boards before wiring. GPIO assignments are reference choices, not a substitute for checking the selected development board.

The task-level alarm bound is less than 100 ms from a completed firmware threshold evaluation to GPIO update. It does not bound sensor response time or the up-to-2-second wait until the next sample.

## Firmware Behavior

- **Sensor task:** pinned to core 0 at application priority 4; samples/updates sensor state on a 2-second `vTaskDelayUntil` schedule. The PMS5003 parser validates frame length and checksum. SCD30 and BME680 values carry validity/freshness flags.
- **Network task:** pinned to core 1 at application priority 1; handles Wi-Fi/MQTT state independently and publishes a latest-state JSON record every 60 seconds. Network retries use bounded backoff.
- **Task boundary:** a length-one FreeRTOS queue uses overwrite semantics, preventing stale telemetry from accumulating and preventing network stalls from blocking acquisition.
- **Alarm:** default CO2 assert/release levels are example project settings (1200/1000 ppm), not universal health guidance. Adjust them for the product requirements. The alarm LED and buzzer are driven locally; invalid sensor data is reported separately in telemetry.
- **Security:** MQTT is TLS-only and fails closed if no broker root CA is configured. PubSubClient publishes at QoS 0; do not assume QoS 1 outbound support. Credentials and CA material belong in an untracked `secrets.h`.
- **Watchdog:** the application tasks register with the ESP-IDF task watchdog and reset it only while making progress. Platform defaults and timeout must be confirmed for the selected PlatformIO platform and tested on hardware.

## Build and Flash

Prerequisites: PlatformIO Core or the VS Code PlatformIO extension, USB data cable, ESP32-WROOM-32 development board, and sensor modules connected as above.

1. Copy `firmware/include/secrets.example.h` to `firmware/include/secrets.h` and set Wi-Fi SSID/password, MQTT host, unique device ID, and the broker's trusted root CA PEM. Do not commit `secrets.h` or private keys.
2. From the repository root, build the firmware:

	```sh
	pio run -d firmware
	```

3. Connect the ESP32 and upload:

	```sh
	pio run -d firmware -t upload
	```

4. Open the serial monitor at 115200 baud:

	```sh
	pio device monitor -d firmware -b 115200
	```

Without valid credentials and a CA certificate, sensor sampling and local alarm operation still run, but Wi-Fi/MQTT telemetry remains disabled. The initial board configuration and pin map must be verified before powering attached hardware.

## Power Simulation

`simulation/power_regulator.cir` is an LTspice-compatible behavioral netlist for a USB input, simplified TP4056-like CC/CV charging behavior, one-cell battery model, external idealized source mux, and dropout-limited 3.3 V LDO/load. It is an architecture study only: it is not a vendor-accurate TP4056 macro-model, cell electrochemical model, charger thermal model, or safety validation. The explicit external mux is important because a TP4056 alone does not provide power-path management. The model also demonstrates that a 3.3 V LDO cannot regulate from the cell throughout its full discharge range.

Open the netlist in LTspice and run the transient analysis; inspect `V(USB)`, `V(BAT)`, `V(SYS)`, `V(3V3)`, and `I(BCHG)`. Replace behavioral elements with selected vendor models and measured loads before using results to make component decisions.

## Verification Plan and Known Limits

Before a hardware release, verify sensor accuracy/freshness, bus recovery, PM frame corruption handling, alarm timing, Wi-Fi loss/recovery, TLS certificate validation, battery source switchover, low-battery behavior, regulator thermal performance, and watchdog recovery. Measure peak and average current with the radio, PMS fan, and indicators in realistic states. The current firmware provides serial diagnostics and cloud telemetry but does not implement a local OLED UI, OTA update, production provisioning UX, persistent configuration, or certified alarm behavior.

The power architecture requires a real charger with power-path management or a separately engineered mux/load-sharing circuit. Do not connect a LiPo to an unreviewed charger/regulator design or leave charging unattended during development. Follow the cell manufacturer's charge limits and applicable product/battery requirements.