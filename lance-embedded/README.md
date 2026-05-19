# LANCE Embedded

Embedded code for LANCE

## Hardware

### Actuators

| Component | Driver | Connector | Control |
|-----------|--------|-----------|---------|
| Linear Actuator 1 - Drill lift (Large) | VNH5019 H-bridge (A1) | J13 | GPIO 5 (FIN), GPIO 4 (RIN) |
| Linear Actuator 2 - Bio vacuum arm (Small) | MPQ6612A H-bridge (A8) | J17 | GPIO 7 (FIN), GPIO 6 (RIN) |
| Drill motor (SparkMax) | PWM servo signal | J19 (signal), J12 (power) | GPIO 16 |
| SCABBARD suction valve (servo, silkscreened SPARK2) | PWM servo signal | J20 (signal), J21 (power) | GPIO 15 |
| Stepper 1 - CITADEL vacuum arm | MP6603 (A6) | J23 (motor), J25 (microstep) | GPIO 48 (STEP), GPIO 47 (DIR) |
| Stepper 2 - LIBS | MP6603 (A7) | J24 (motor), J26 (microstep) | GPIO 41 (STEP), GPIO 40 (DIR) |

### Sensors

| Component | Interface | Connector | Pins |
|-----------|-----------|-----------|------|
| SHT30 (temp/humidity) | I2C (0x44) | J16 | GPIO 1 (SDA), GPIO 0 (SCL) |
| Battery voltage divider | ADC | - | GPIO 19 |
| 12V voltage divider | ADC | - | GPIO 20 |
| 5V voltage divider | ADC | - | GPIO 2 |

### Other

| Component | Connector | Control |
|-----------|-----------|---------|
| Laser | J15 | GPIO 43 (NMOS gate) |
| CAN bus (TJA1051T) | J3 | GPIO 9 (TX), GPIO 8 (RX) |
| Camera 1 power | J6 | Power only |
| Camera 2 power | J9 | Power only |
| SCABBARD fan power (uses CITADEL fan) | J22 | Power only |

### Power

| Connector | Source |
|-----------|--------|
| J1 | 4S LiPo battery |
| J2 | 12V input |
| A4 (D24V22F5) | 5V regulator |

## Serial Commands

All commands are comma-delimited.

| Command | Description |
|---------|-------------|
| `ping` | Responds with `pong` |
| `time` | Responds with `millis()` |
| `led` | Toggles built-in LED |
| `linac,<id>,<duty>` | Control linear actuator (1=drill lift, 2=bio vacuum arm; duty: -1.0=full retract, 0=stop, 1.0=full extend) |
| `drill,<duty>` | Control drill SparkMax (duty: -1.0 to 1.0) |
| `valve,<degrees>` | Control SCABBARD valve servo (degrees: 0-180) |
| `stepper,<id>,<degrees>` | Rotate stepper (id: 1-2, degrees: float) |
| `laser,<0\|1>` | Laser off/on |
| `sht` | Read SHT30 temperature and humidity |
| `stop` | Emergency stop all actuators and laser |
| `shutdown` | Stop everything and detach servos |
| `can_relay_tovic,...` | Relay a CAN frame (see VicCAN docs) |
| `can_relay_mode,<on\|off>` | Enable/disable CAN relay mode |

## CAN Commands (VicCAN)

LANCE uses MCU ID 5 (`MCU_LANCE`, formerly `MCU_FAERIE`).

| Command ID | Name | Description |
|------------|------|-------------|
| 1 | `CMD_PING` | Responds with 1 |
| 2 | `CMD_TIME` | Responds with millis() |
| 3 | `CMD_B_LED` | Set built-in LED |
| 6 | `CMD_ALL_STOP` | Emergency stop |
| 7, 8 | `CMD_VERSION_*` | Report firmware version |
| 19 | `CMD_REV_SET_DUTY` | Drill SparkMax duty (1f64: duty -1.0..1.0) |
| 25 | `CMD_PWMSERVO_SET_DEG` | SCABBARD valve servo position (1f64: angle 0-180) |
| 27 | `CMD_STEPPER_CTRL` | Stepper rotate (2f32: stepper_id, degrees) |
| 28 | `CMD_LASER_CTRL` | Laser on/off (1f64: 0 or 1) |
| 42 | `CMD_LANCE_LINEAR_AC` | Linear actuator (2f32: linac_id, duty -1.0..1.0) |

### Periodic Feedback

| Command ID | Name | Interval | Data |
|------------|------|----------|------|
| 54 | `CMD_POWER_VOLTAGE` | 1s | 4i16: vbatt\*100, v12\*100, v5\*100, 0 |
| 57 | `CMD_SHT_TEMP_HUM` | 2s | 2f32: temperature_C, humidity_% |

## Building

```sh
nix develop   # from biosensor-embedded root
pio run -d lance-embedded -e lance_prod    # production build
pio run -d lance-embedded -e lance_dev     # dev build (local libs)
pio run -d lance-embedded -e lance_prod -t upload   # flash
pio device monitor -d lance-embedded       # serial monitor
```

## Responsible People

| Name | Email | Responsibilities |
| Riley McLain | <rjm0037@uah.edu> | Author, Maintainer |

