/**
 * @file main.cpp
 * @author Jack Schumacher (js0342@uah.edu)
 * @author David Sharpe (ds0196@uah.edu)
 * @brief ASTRA Biosensor Citadel embedded code
 *
 */
#include <Arduino.h>
#include <ESP32Servo.h>

#include <cmath>
#include <typeinfo>

#include "AstraMisc.h"
#include "AstraMotors.h"
#include "AstraREVCAN.h"
#include "AstraVicCAN.h"

// Remove to disable the boards inbuilt LED blinking
#define BLINK
#define CAN_TX 16
#define CAN_RX 17

#define REV_PWM_MIN 1000 // us  -1.0 duty
#define REV_PWM_MAX 2000 // us  1.0 duty

#define SPARK_PWM 4

#define COMMS_UART Serial // To/from USB for debugging

bool ledState = false;

// Defined servos (3 for valves, 3 for distributors, 3 for chemicals)
Servo valve0, valve1, valve2, distributor0, distributor1, distributor2, chemical0, chemical1, chemical2;

// Track the current distributor positions and the requested distributor positions
int distributorPos[3] = {0, 0, 0};
int distributorReq[3] = {0, 0, 0};
Servo* valves[3] = {&valve0, &valve1,&valve2};
Servo* chemicals[3] = {&chemical0, &chemical1,&chemical2};

long lastWiggle = 0; // For Distributor servos- count last time moved

unsigned long lastCtrlCmd = 0;
unsigned long lastMotorStatus = 0;

// Variables for CAN commands- since all servos in a group should be writing the same
int valveID;
int chemicalID;
int millimetersToMove;
int distributorID;

// Control the NEO550 functioning as the fan motor
Servo fanMotor;

void setup()
{
    // Servo Pins: 13,14,18,19,22,23,25,26,27
    // Actual pins From bottom facing the USB C port- 25,13,27,18,22
    // Top Left: 14,26,19,23 (last 2 on the top are not used)
    Serial.begin(SERIAL_BAUD);
    // Valves are on top of the unit
    valve0.attach(13);
    valve1.attach(14);
    valve2.attach(18);

    // Distributors are on each of the pieces that hang down
    distributor0.attach(19);
    distributor1.attach(22);
    distributor2.attach(23);

    // Chemical servos are in the back of the unit (the larger servos)
    chemical0.attach(25);
    chemical1.attach(26);
    chemical2.attach(27);

    fanMotor.attach(SPARK_PWM, REV_PWM_MIN, REV_PWM_MAX);

    // Set all valve, distributor, and chemical servos to their 0 positions
    valve0.write(0);
    valve1.write(0);
    valve2.write(0);

    distributor0.write(0);
    distributor1.write(0);
    distributor2.write(0);

    chemical0.write(0);
    chemical1.write(0);
    chemical2.write(0);

    fanMotor.writeMicroseconds((REV_PWM_MIN + REV_PWM_MAX) / 2);

    if (ESP32Can.begin(TWAI_SPEED_1000KBPS, CAN_TX, CAN_RX))
        Serial.println("CAN bus started!");
    else
        Serial.println("CAN bus failed!");
}

void loop()
{

    // Motor control safety timeout- if no command is received in 1 second, shut off the NEO
    if (millis() - lastCtrlCmd > 1000)
    {
        lastCtrlCmd = millis();
        fanMotor.writeMicroseconds((REV_PWM_MIN + REV_PWM_MAX) / 2);
        Serial.println("CITADEL Fan Motor - Safety Timeout.");
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

        else if (command == "led") // This command will not work when using boards that do not have a inbuilt
                                   // LED (i.e. the ESP32 Dev Module 1)
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

        else if (commandID == CMD_REV_SET_DUTY)
        { // Converts duty cycle input into writeMicroseconds range of the NEO
            if (canData.size() == 1)
            {
                lastCtrlCmd = millis();
                float percent = canData[0] / 100.0;
                // Limit to 50% duty cycle per Kade's request
                if (percent < -0.5)
                {
                    percent = -0.5;
                }
                else if (percent > 0.5)
                {
                    percent = 0.5;
                }
                int value = map_d(percent, -1.0, 1.0, REV_PWM_MIN, REV_PWM_MAX);
                fanMotor.writeMicroseconds(value);
                Serial.print("Setting REV duty to ");
                Serial.println(value);
            }
        }
        else if (commandID == CMD_REV_STOP)
        {
            lastCtrlCmd = millis();
            fanMotor.writeMicroseconds((REV_PWM_MIN + REV_PWM_MAX) / 2);
        }

        if (commandID == 40)
        {
            if (canData.size() == 1)
            {
                valveID = canData[0];
            }
            if (canData.size() == 2)
            {
                chemicalID = canData[0];
                millimetersToMove = canData[1];
                // Multiply for 55 divide by 10 for map - TBD
                millimetersToMove = ((millimetersToMove * 55) / 30);
                if (millimetersToMove >= 75)
                {
                    millimetersToMove = 75;
                }
            }
            if (canData.size() == 4)
            {
                for (int i = 0; i < 3; i++)
                {
                    distributorReq[i] = canData[i];
                }
            }
            // If -1 is passed in, close all valves
            // Valve movement
            if(valveID >=0 && valveID <= 2){
                valves[valveID]->write(180);
            }
            // If the valve IDs are not valid or -1 is passed in, loop through and close all valves
            else
            {
                for(int i=0; i <3; i++){
                    valves[i]->write(0);
                }
            }

            if(chemicalID >=0 && chemicalID <= 2){
                chemicals[chemicalID]->write(millimetersToMove);
            }
            


            
        }
    }
    // Wiggle every 500ms
    if ((millis() - lastWiggle > 500) && (millis() - lastWiggle < 2000))
    {
        // Max movement for these servos is 100 degrees due to hardware mounting limit
        lastWiggle = millis();

        distributorPos[0] = distributorReq[0] && !distributorPos[0];
        distributor0.write(distributorPos[0] ? 100 : 0);

        distributorPos[1] = distributorReq[1] && !distributorPos[1];
        distributor1.write(distributorPos[1] ? 100 : 0);

        distributorPos[2] = distributorReq[2] && !distributorPos[2];
        distributor2.write(distributorPos[2] ? 100 : 0);
    }
}
