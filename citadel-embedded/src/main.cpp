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
#include "AstraMisc.h"
#include "AstraVicCAN.h"
#include "AstraMotors.h"

// Remove to disable the boards inbuilt LED blinking
#define BLINK
#define CAN_TX 34
#define CAN_RX 35

#define FAN_MOTOR_ID 1 // TODO: Needs to be confirmed

bool ledState = false;

// Defined servos (3 for valves, 3 for distributors, 3 for chemicals)
Servo valve1, valve2, valve3, distributor1, distributor2, distributor3, chemical1, chemical2, chemical3;
Servo *servoReference[9] = {&valve1, &valve2, &valve3, &distributor1, &distributor2, &distributor3, &chemical1, &chemical2, &chemical3};

int currentServoPos[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int targetServoPos[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned long lastServoMoveTime = 0;
int servoSpeed = 18; // Uses servo steps to determine speed

long lastWiggle = 0; // For PWM servos
bool servoStates[9] = {false, false, false, false, false, false, false, false, false};

uint32_t lastBlink = 0;
unsigned long lastAccel = 0;
unsigned long lastHB = 0;
int heartBeatNum = 1;
unsigned long lastCtrlCmd = 0;
unsigned long lastMotorStatus = 0;

AstraMotors FanMotor(FAN_MOTOR_ID, sparkMax_ctrlType::kDutyCycle, true);

// Declarations
void Stop();
void Brake(bool enable);

void setup()
{
  // Servo Pins: 13,14,18,19,22,23,25,26,27
  // Actual pins From bottom facing the USB C port- 25,13,27,18,22
  // Top Left: 14,26,19,23 (last 2 on the top are not used)
  Serial.begin(SERIAL_BAUD);
  // Valves are on top of the unit
  valve1.attach(13);
  valve2.attach(14);
  valve3.attach(18);

  // Distributors are on each of the pieces that hang down
  distributor1.attach(19);
  distributor2.attach(22);
  distributor3.attach(23);

  // Chemical servos are in the back of the unit (the larger servos)
  chemical1.attach(25);
  chemical2.attach(26);
  chemical3.attach(27);

  if (ESP32Can.begin(TWAI_SPEED_1000KBPS, CAN_TX, CAN_RX))
    Serial.println("CAN bus started!");
  else
    Serial.println("CAN bus failed!");
}

void loop()
{
  // put your main code here, to run repeatedly:
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

  if (millis() - lastWiggle > 1000)
  {
    lastWiggle = millis();
    for (int i = 0; i < 9; i++)
    {
      if (servoStates[i])
      {
        targetServoPos[i] = (targetServoPos[i] == 0 ? 180 : 0);
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

      //--------//
      //  Misc  //
      //--------//
      /**/
      if (command == "ping")
      {
        Serial.println("pong");
      }

      else if (command == "time")
      {
        Serial.println(millis());
      }

      else if (command == "led") // This command will not work when using boards that do not have a inbuilt LED (i.e. the Dev Module 1)
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

        if (servo_id >= 0 && servo_id <= 9)
        { // Validate the servo values
          if (servo_id >= 3 && servo_id < 9)
          {
            targetServoPos[servo_id] = (servo_angle <= 60) ? servo_angle : 60;
          }
          else
          {
            targetServoPos[servo_id] = servo_angle;
          }
        }
      }
      else if (args[0] == "val")
      {
        valve1.write(args[1].toInt());
        valve2.write(args[1].toInt());
        valve3.write(args[1].toInt());
      }
      else if (args[0] == "dist")
      {
        distributor1.write(args[1].toInt());
        distributor2.write(args[1].toInt());
        distributor3.write(args[1].toInt());
      }
      else if (args[0] == "chem")
      {
        chemical1.write(args[1].toInt());
        chemical2.write(args[1].toInt());
        chemical3.write(args[1].toInt());
      }
      else if (args[0] == "all")
      {
        valve1.write(args[1].toInt());
        valve2.write(args[1].toInt());
        valve3.write(args[1].toInt());
        distributor1.write(args[1].toInt());
        distributor2.write(args[1].toInt());
        distributor3.write(args[1].toInt());
        chemical1.write(args[1].toInt());
        chemical2.write(args[1].toInt());
        chemical3.write(args[1].toInt());
      }
      // else if (args[0] == "shutdown")
      // {
      //   // TODO: Add pins here
      //   valve1.detach();
      //   valve2.detach();
      //   valve3.detach();

      //   distributor1.detach();
      //   distributor2.detach();
      //   distributor3.detach();

      //   chemical1.detach();
      //   chemical2.detach();
      //   chemical3.detach();
      // }
    }

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

      // Misc

      if (commandID == CMD_PING)
      {
        vicCAN.respond(1); // "pong"
        Serial.println("Received ping over CAN");
      }
      else if (commandID == CMD_PWMSERVO_SET_DEG)
      {
        if (canData.size() == 2 && canData[0] > 0 && canData[0] < 9)
        {
          unsigned servoId = static_cast<unsigned>(canData[0]);
          targetServoPos[servoId - 1] = static_cast<int>(canData[1]);
        }
      }
      // Accelerate motors; update the speed for all motors
      if (millis() - lastAccel >= 50)
      {
        lastAccel = millis();
        for (int i = 0; i < 4; i++)
        {
          FanMotor.accelerate();
        }
      }
      else
      {
      }

      // Heartbeat for REV motors
      if (millis() - lastHB >= 3)
      {
        lastHB = millis();
        CAN_sendHeartbeat(heartBeatNum);
        heartBeatNum++;
        if (heartBeatNum > 4)
        {
          heartBeatNum = 1;
        }
      }

      // Safety timeout
      if (millis() - lastCtrlCmd > 2000) // if no control commands are received for 2 seconds
      {
        lastCtrlCmd = millis();

        // Only ignore safety timeout if all motors are rotating
        bool allRotating = true;
        for (int i = 0; i < 1; i++)
        {
          if (!FanMotor.isRotToPos())
          {
            allRotating = false;
            break;
          }
        }
        if (!allRotating)
        {
          Serial.println("No Control, Safety Timeout");
          Stop();
        }
      }
    }
  }
}

void Brake(bool enable)
{
  FanMotor.setBrake(enable);
}
void Stop()
{
  FanMotor.stop();
}
