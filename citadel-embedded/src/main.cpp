/**
 * @file main.cpp
 * @author Jack Schumacher (js0342@uah.edu)
 * @author David Sharpe (ds0196@uah.edu)
 * @brief ASTRA Biosensor Citadel embedded code
 *
 */
#include <Arduino.h>
#include <cmath>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "AstraMisc.h"
#include "AstraVicCAN.h"
#include "AstraREVCAN.h"
#include "AstraMotors.h"

// Remove to disable the boards inbuilt LED blinking
#define BLINK
#define CAN_TX 33
#define CAN_RX 35

#define FAN_MOTOR_ID 1   // TODO: Needs to be confirmed that this is the correct CAN ID
#define REV_PWM_MIN 1000 // us  -1.0 duty
#define REV_PWM_MAX 2000 // us  1.0 duty

#define SPARK_PWM 26

#define COMMS_UART Serial // To/from USB for debugging

#define SERVOMIN 150  // This is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX 600  // This is the 'maximum' pulse length count (out of 4096)
#define USMIN 600     // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX 2400    // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

bool ledState = false;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Defined servos (3 for valves, 3 for distributors, 3 for chemicals)
// TODO: Assign these when the board is assembled
const uint8_t valve1 = 0;
const uint8_t valve2 = 1;
const uint8_t valve3 = 2;
const uint8_t distributor1 = 3;
const uint8_t distributor2 = 4;
const uint8_t distributor3 = 5;
const uint8_t chemical1 = 6;
const uint8_t chemical2 = 7;
const uint8_t chemical3 = 8;

uint8_t servos[] = {valve1, valve2, valve3, distributor1, distributor2, distributor3, chemical1, chemical2, chemical3};

int currentServoPos[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int targetServoPos[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned long lastServoMoveTime = 0;
int servoSpeed = 10; // Uses servo steps to determine speed

long lastWiggle = 0; // For PWM servos

unsigned long lastAccel = 0;

unsigned long lastHB = 0;
int heartBeatNum = 1;

unsigned long lastCtrlCmd = 0;
unsigned long lastMotorStatus = 0;

// Variables for CAN commands- since all servos in a group should be writing the same
int tubeID;
int valveAngle;
int distributorAngle;
int chemicalAngle;

// Control the NEO550 functioning as the fan motor
Servo fanMotor;

void loop2(void *pvParameters)
{
  while (true)
  {
    CAN_sendHeartbeat(heartBeatNum);
    heartBeatNum++;
    if (heartBeatNum > 4)
    {
      heartBeatNum = 1;
    }
    delay(5);
  }
}

// Declarations
void Stop();

void setup()
{
  // Servo Pins: 13,14,18,19,22,23,25,26,27
  // Actual pins From bottom facing the USB C port- 25,13,27,18,22
  // Top Left: 14,26,19,23 (last 2 on the top are not used)
  Serial.begin(SERIAL_BAUD);
  // Set up pwm
  pwm.begin();
  pwm.setPWMFreq(60);

  fanMotor.attach(SPARK_PWM, REV_PWM_MIN, REV_PWM_MAX);

  if (ESP32Can.begin(TWAI_SPEED_1000KBPS, CAN_TX, CAN_RX))
    Serial.println("CAN bus started!");
  else
    Serial.println("CAN bus failed!");

  // TODO: Confirm that this is working- is CAN receiving a heartbeat?
  // Pin the CAN heartbeat task to core
  xTaskCreatePinnedToCore(
      loop2,   // Function to implement the task
      "loop2", // Name of the task
      1000,    // Stack size in bytes
      NULL,    // Task input parameter
      0,       // Priority of the task
      NULL,    // Task handle.
      0        // Core where the task should run
  );
}
void setServoAngle(uint8_t channel, double angle)
{
  int servoPulseLength = map(angle, 0, 180, SERVOMIN, SERVOMAX);
  pwm.setPWM(channel, 0, servoPulseLength);
}

void loop()
{

  if (millis() - lastServoMoveTime >= servoSpeed)
  {
    lastServoMoveTime = millis();

    for (int i = 0; i < 9; i++)
    {
      if (currentServoPos[i] < targetServoPos[i])
      {
        currentServoPos[i]++;
        servoReference[i]->write(currentServoPos[i]);
      }
      else if (currentServoPos[i] > targetServoPos[i])
      {
        currentServoPos[i]--;
        servoReference[i]->write(currentServoPos[i]);
      }
    }
  }

  if (millis() - lastWiggle > 500)
  {
    // Move Servos ID 2-5 (The Distributor Servos) back and fourth to distribute the dirt into the tubes
    lastWiggle = millis();
    for (int i = 3; i < 6; i++)
    {
      if (currentServoPos[i])
      {
        targetServoPos[i] = (targetServoPos[i] == 0 ? 180 : 0);
      }
    }
  }
  // Serial commands
  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');
    Serial.println(input);

    input.trim();                  // Remove preceding and trailing whitespace
    std::vector<String> args = {}; // Initialize empty vector to hold separated arguments
    parseInput(input, args);       // Separate `input` by commas and place into args vector
    args[0].toLowerCase();         // Make command case-insensitive
    String command = args[0];      // To make processing code more readable

    if (command == "ping")
    {
      Serial.println("pong");
    }

    else if (command == "time")
    {
      Serial.println(millis());
    }

    else if (command == "led") // This command will not work when using boards that do not have a inbuilt LED (i.e. the ESP32 Dev Module 1)
    {
      digitalWrite(LED_BUILTIN, !ledState);
      ledState = !ledState;
    }

    else if (args[0] == "can_relay_tovic")
    {
      vicCAN.relayFromSerial(args);
    }

    else if (args[0] == "can_relay_mode")
    {
      if (args[1] == "on")
      {
        vicCAN.relayOn();
      }
      else if (args[1] == "off")
      {
        vicCAN.relayOff();
      }
    }
    // Servos are numbered 1-9
    // To use: servo,[servo number],[degrees]
    else if (args[0] == "servo")
    {
      int servo_id = args[1].toInt() - 1; // Get servo id
      int servo_angle = args[2].toInt();

      if (servo_id >= 0 && servo_id < 9)
      {
        // Validate the servo values - Servo IDs 6-9 (the chemical servos) should only move 60 degrees to prevent overextension and other issues

        if (servo_id >= 1 && servo_id <= 3)
        {
          targetServoPos[servo_id] = (servo_angle <= 60) ? servo_angle : 60;
        }
        // Other servos can move the full 180 degrees
        else
        {
          targetServoPos[servo_id] = servo_angle;
        }
      }
    }
    // Commands that move all servos in a group
    //  val = valve servos
    //  dist = distributor servos
    //  chem = chemical servos
    //  all = all servos (might cause power issues)

    else if (args[0] == "val")
    {
      setServoAngle(valve1, args[1].toInt());
      setServoAngle(valve2, args[1].toInt());
      setServoAngle(valve3, args[1].toInt());
    }
    else if (args[0] == "dist")
    {
      setServoAngle(distributor1, args[1].toInt());
      setServoAngle(distributor2, args[1].toInt());
      setServoAngle(distributor3, args[1].toInt());
    }
    else if (args[0] == "chem")
    {

      if (args[1].toInt() >= 55)
      {
        setServoAngle(chemical1, 55);
        setServoAngle(chemical2, 55);
        setServoAngle(chemical2, 55);
      }
      else
      {
        setServoAngle(chemical1, args[1].toInt());
        setServoAngle(chemical2, args[1].toInt());
        setServoAngle(chemical3, args[1].toInt());
      }
    }
    else if (args[0] == "all")
    {
      setServoAngle(valve1, args[1].toInt());
      setServoAngle(valve2, args[1].toInt());
      setServoAngle(valve3, args[1].toInt());
      setServoAngle(distributor1, args[1].toInt());
      setServoAngle(distributor2, args[1].toInt());
      setServoAngle(distributor3, args[1].toInt());

      if (args[1].toInt() >= 55)
      {
        setServoAngle(chemical1, 55);
        setServoAngle(chemical2, 55);
        setServoAngle(chemical3, 55);
      }
      else
      {
        setServoAngle(chemical1, args[1].toInt());
        setServoAngle(chemical2, args[1].toInt());
        setServoAngle(chemical3, args[1].toInt());
      }
    }
  }

  // CAN
  if (vicCAN.readCan())
  {
    const uint8_t commandID = vicCAN.getCmdId();
    static std::vector<double> canData;
    vicCAN.parseData(canData);

    Serial.print("VicCAN: ");
    Serial.print(commandID);
    Serial.print("; ");
    if (canData.size() > 0)
    {
      for (const double &data : canData)
      {
        Serial.print(data);
        Serial.print(", ");
      }
    }
    Serial.println();

    // Misc CAN commands

    if (commandID == CMD_PING)
    {
      vicCAN.respond(1); // "pong"
      Serial.println("Received ping over CAN");
    }

    if (commandID == 40)
    {
      
      tubeID = canData[0];
      valveAngle = canData[1];
      distributorAngle = canData[2];
      chemicalAngle = canData[3];
      if (chemicalAngle >= 55)
      {
        chemicalAngle = 55;
      }
      // Valve group
      if (tubeID == 1)
      {
        setServoAngle(valve1, valveAngle);
        setServoAngle(valve2, valveAngle);
        setServoAngle(valve3, valveAngle);
        setServoAngle(distributor1, distributorAngle);
        setServoAngle(distributor2, distributorAngle);
        setServoAngle(distributor3, distributorAngle);
        setServoAngle(chemical1,chemicalAngle);
        setServoAngle(chemical2,chemicalAngle);
        setServoAngle(chemical3,chemicalAngle);
      }
      // Distributor Group
      else if (tubeID == 2)
      {
        valve1.write(valveAngle);
        valve2.write(valveAngle);
        valve3.write(valveAngle);
        distributor1.write(distributorAngle);
        distributor2.write(distributorAngle);
        distributor3.write(distributorAngle);
        chemical1.write(distributorAngle);
        chemical2.write(distributorAngle);
        chemical3.write(distributorAngle);
      }
      // Chemical group
      else if (tubeID == 3)
      {
        valve1.write(valveAngle);
        valve2.write(valveAngle);
        valve3.write(valveAngle);
        distributor1.write(distributorAngle);
        distributor2.write(distributorAngle);
        distributor3.write(distributorAngle);
        chemical1.write(distributorAngle);
        chemical2.write(distributorAngle);
        chemical3.write(distributorAngle);
      }
    }
    // Converts duty cycle input into writeMicroseconds range of the NEO
    if (commandID == 19)
    {
      if (canData.size() == 1)
      {
        lastCtrlCmd = millis();
        int value = map_d(canData[0] / 100.0, -1.0, 1.0, REV_PWM_MIN, REV_PWM_MAX);
        fanMotor.writeMicroseconds(value);
        Serial.print("Setting REV duty to ");
        Serial.println(value);
      }
    }

    // Motor control safety timeout- if no command is received in 2 seconds, shut off the NEO
    if (millis() - lastCtrlCmd > 2000)
    {
      lastCtrlCmd = millis();
      fanMotor.write(0);
    }
    else if (commandID == CMD_REV_STOP)
    {
      lastCtrlCmd = millis();
      fanMotor.write((REV_PWM_MIN + REV_PWM_MAX) / 2);
    }
  }
}
