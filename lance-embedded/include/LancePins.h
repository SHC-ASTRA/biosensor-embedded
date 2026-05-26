/**
 * @file LancePins.h
 * @brief Pin definitions for LANCE (ESP32-S3 DevKit)
 */
#pragma once

// CAN Bus (TJA1051T transceiver)
#define PIN_CAN_RX 8
#define PIN_CAN_TX 9

// I2C (SHT30 temp/humidity sensor)
#define PIN_I2C_SCL 0
#define PIN_I2C_SDA 1

// ADC Voltage Dividers
#define PIN_ADC_VBATT 19
#define PIN_ADC_12V 20
#define PIN_ADC_5V 2

// Linear Actuators (H-bridge: FIN=forward/extend, RIN=reverse/retract)
#define PIN_LINAC_LARGE_FIN 5  // Drill lift (large) - VNH5019
#define PIN_LINAC_LARGE_RIN 4
#define PIN_LINAC_SMALL_FIN 7  // Bio vacuum arm (small) - MPQ6612A
#define PIN_LINAC_SMALL_RIN 6

// Motor/Servo PWM (active low accent: silkscreened as SPARK1/SPARK2)
#define PIN_DRILL_PWM 16  // SparkMax for drill motor
#define PIN_VALVE_PWM 15  // Servo for SCABBARD suction valve (not a SparkMax)

// Stepper Motors (STEP + DIR)
#define PIN_STEPPER1_STEP 48  // CITADEL vacuum arm
#define PIN_STEPPER1_DIR 47
#define PIN_STEPPER2_STEP 41  // LIBS
#define PIN_STEPPER2_DIR 40

// Laser (NMOS gate)
#define PIN_LASER_NMOS 43
