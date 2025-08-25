/**
 * @file main.cpp
 * @author Charles (you@domain.com)
 * @author Karl (you@domain.com)
 * @author David (you@domain.com)
 * @brief Controls Citadel's fans, servos, LSS, vibration motor, and relays commands for steppers
 *
 */


//------------//
//  Includes  //
//------------//

#include <Arduino.h>
#include <cmath>
#include <LSS.h>
#include <ESP32Servo.h>
#include <AccelStepper.h>

#include "AstraMisc.h"
#include "AstraVicCAN.h"
#include "CitadelMainMCU.h"
#include "DRV8825.h"


//------------//
//  Settings  //
//------------//

// Comment out to disable LED blinking
#define BLINK

#define LSS_ID 254


//---------------------//
//  Component classes  //
//---------------------//

LSS myLSS = LSS(LSS_ID);

Servo servo1, servo2, servo3;

DRV8825 stepper1(200, PIN_STEP_DIR, PIN_STEP_1, 0, 0, 0, 0);
DRV8825 stepper2(200, PIN_STEP_DIR, PIN_STEP_2, 0, 0, 0, 0);
DRV8825 stepper3(200, PIN_STEP_DIR, PIN_STEP_3, 0, 0, 0, 0);
DRV8825 stepper4(200, PIN_STEP_DIR, PIN_STEP_4, 0, 0, 0, 0);

DRV8825* steppers[4] = {&stepper1, &stepper2, &stepper3, &stepper4};


//----------//
//  Timing  //
//----------//

bool ledState = false;

long fansTimer, fanTimer_1, fanTimer_2, fanTimer_3;
bool fansOn = 0, fanOn_1 = 0, fanOn_2 = 0, fanOn_3 = 0;

long prevFanTime = 0, prevFanTime_1 = 0, prevFanTime_2 = 0, prevFanTime_3 = 0;

bool pumpStates[4] = {false, false, false, false};
float pumpSteps[4] = {0, 0, 0, 0};

long lastWiggle = 0;  // For PWM servos
bool servoStates[3] = {false, false, false};
int servoPositions[3] = {0, 0, 0};

long lastVoltRead = 0;


void stepperController(void* pvParameters) {
    while (true) {
        for (int i = 0; i < 4; i++) {
            if (pumpStates[i]) {
                digitalWrite(PIN_STEP_ENABLE, LOW);
                steppers[i]->rotate(pumpSteps[i]);
                digitalWrite(PIN_STEP_ENABLE, HIGH);
                pumpStates[i] = false;
                pumpSteps[i] = 0;
            }
        }
        delay(5);
    }
}


//--------------//
//  Prototypes  //
//--------------//


//------------------------------------------------------------------------------------------------//
//  Setup
//------------------------------------------------------------------------------------------------//
//
//
//------------------------------------------------//
//                                                //
//      ////////    //////////    //////////      //
//    //                //        //        //    //
//    //                //        //        //    //
//      //////          //        //////////      //
//            //        //        //              //
//            //        //        //              //
//    ////////          //        //              //
//                                                //
//------------------------------------------------//
void setup()
{
    //--------//
    //  Pins  //
    //--------//

    pinMode(LED_BUILTIN, OUTPUT);

    // Fans
    pinMode(PIN_FAN_1, OUTPUT);
    pinMode(PIN_FAN_2, OUTPUT);
    pinMode(PIN_FAN_3, OUTPUT);
    digitalWrite(PIN_FAN_1, LOW);
    digitalWrite(PIN_FAN_2, LOW);
    digitalWrite(PIN_FAN_3, LOW);

    // Vibrator
    pinMode(PIN_VIBMOTOR, OUTPUT);
    digitalWrite(PIN_VIBMOTOR, LOW);

    // Stepper Enable
    pinMode(PIN_STEP_ENABLE, OUTPUT);
    digitalWrite(PIN_STEP_ENABLE, HIGH);


    //------------------//
    //  Communications  //
    //------------------//

    Serial.begin(SERIAL_BAUD);

    ESP32Can.begin(TWAI_SPEED_1000KBPS, CAN_TX, CAN_RX);


    //-----------//
    //  Sensors  //
    //-----------//


    //--------------------//
    //  Misc. Components  //
    //--------------------//

    servo1.attach(PIN_PWM_SERVO_1);
    servo2.attach(PIN_PWM_SERVO_2);
    servo3.attach(PIN_PWM_SERVO_3);

    LSS::initBus(LYNX_SERIAL, LSS_BAUD);
    myLSS.setAngularStiffness(0);
    myLSS.setAngularHoldingStiffness(0);
    myLSS.setAngularAcceleration(15);
    myLSS.setAngularDeceleration(15);
    Serial.println("Smart servo has started");

    stepper1.begin();
    stepper2.begin();
    stepper3.begin();
    stepper4.begin();

    xTaskCreatePinnedToCore (
        stepperController,     // Function to implement the task
        "stepperController",   // Name of the task
        1000,      // Stack size in bytes
        NULL,      // Task input parameter
        0,         // Priority of the task
        NULL,      // Task handle.
        0          // Core where the task should run
    );
}


//------------------------------------------------------------------------------------------------//
//  Loop
//------------------------------------------------------------------------------------------------//
//
//
//-------------------------------------------------//
//                                                 //
//    /////////      //            //////////      //
//    //      //     //            //        //    //
//    //      //     //            //        //    //
//    ////////       //            //////////      //
//    //      //     //            //              //
//    //       //    //            //              //
//    /////////      //////////    //              //
//                                                 //
//-------------------------------------------------//
void loop()
{
    //----------//
    //  Timers  //
    //----------//

    if (fansOn && (millis() - prevFanTime >= fansTimer))
    {
        digitalWrite(PIN_FAN_1, LOW);
        digitalWrite(PIN_FAN_2, LOW);
        digitalWrite(PIN_FAN_3, LOW);
        fansOn = 0;
        fansTimer = 0;
    }
    else if (fanOn_1 && (millis() - prevFanTime_1 >= fanTimer_1))
    {
        digitalWrite(PIN_FAN_1, LOW);
        fanOn_1 = 0;
        fanTimer_1 = 0;
    }
    else if (fanOn_2 && (millis() - prevFanTime_2 >= fanTimer_2))
    {
        digitalWrite(PIN_FAN_2, LOW);
        fanOn_2 = 0;
        fanTimer_2 = 0;
    }
    else if (fanOn_3 && (millis() - prevFanTime_3 >= fanTimer_3))
    {
        digitalWrite(PIN_FAN_3, LOW);
        fanOn_3 = 0;
        fanTimer_3 = 0;
    }

    if (millis() - lastWiggle > 1000) {
        lastWiggle = millis();
        if (servoStates[0]) {
            servoPositions[0] = (servoPositions[0] == 0 ? 180 : 0);
            servo1.write(servoPositions[0]);
        }
        if (servoStates[1]) {
            servoPositions[1] = (servoPositions[1] == 0 ? 180 : 0);
            servo2.write(servoPositions[1]);
        }
        if (servoStates[2]) {
            servoPositions[2] = (servoPositions[2] == 0 ? 180 : 0);
            servo3.write(servoPositions[2]);
        }
    }

    if (millis() - lastVoltRead > 1000) {
        lastVoltRead = millis();
        float vBatt = convertADC(analogRead(PIN_VDIV_BATT), 10, 2.21);
        float v12 = convertADC(analogRead(PIN_VDIV_12V), 10, 3.32);
        float v5 = convertADC(analogRead(PIN_VDIV_5V), 10, 10);

        vicCAN.send(CMD_POWER_VOLTAGE, vBatt * 100, v12 * 100, v5 * 100);
    }


    //-------------//
    //  CAN input  //
    //-------------//
    //
    //
    //-------------------------------------------------------//
    //                                                       //
    //      /////////          //\\          //\\      //    //
    //    //                  //  \\         // \\     //    //
    //    //                 //    \\        //  \\    //    //
    //    //                /////\\\\\       //   \\   //    //
    //    //               //        \\      //    \\  //    //
    //    //              //          \\     //     \\ //    //
    //      /////////    //            \\    //      \\//    //
    //                                                       //
    //-------------------------------------------------------//

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

        // Misc Physical Control

        else if (commandID == CMD_LSS_TURNBY_DEG)
        {
            if (canData.size() == 1)
            {
                if (canData[0] <= 100 && canData[0] >= -100)
                {
                    myLSS.wheel(canData[0]);
                }
            }
        }

        else if (commandID == CMD_PWMSERVO_SET_DEG)
        {
            if (canData.size() == 2 && canData[0] > 0 && canData[0] < 4) {
                unsigned servoId = static_cast<unsigned>(canData[0]);
                servoStates[servoId - 1] = static_cast<bool>(canData[1]);
            }
        }

        else if (commandID == CMD_DCMOTOR_CTRL)
        {
            if (canData.size() == 1)
            {
                if (canData[0] == 1)
                    digitalWrite(PIN_VIBMOTOR, HIGH);
                else if (canData[0] == 0)
                    digitalWrite(PIN_VIBMOTOR, LOW);
            }
        }

        else if (commandID == CMD_STEPPER_CTRL)
        {
            if (canData.size() == 2 && canData[0] > 0 && canData[0] < 5) {
                const float stepsF = (canData[1] / 1.14009) * 3600.0;
                pumpStates[static_cast<int>(canData[0]) - 1] = true;
                pumpSteps[static_cast<int>(canData[0]) - 1] = stepsF;
            }
        }

        else if (commandID == CMD_LSS_RESET) {
            myLSS.reset();
        }

        // Submodule Specific

        else if (commandID == CMD_CITADEL_FAN_CTRL) {
            if (canData.size() == 2) {
                switch (static_cast<int>(canData[0])) {
                case 1:
                    digitalWrite(PIN_FAN_1, HIGH);
                    fanOn_1 = 1;
                    fanTimer_1 = canData[1];
                    prevFanTime_1 = millis();
                    break;
                case 2:
                    digitalWrite(PIN_FAN_2, HIGH);
                    fanOn_2 = 1;
                    fanTimer_2 = canData[1];
                    prevFanTime_2 = millis();
                    break;
                case 3:
                    digitalWrite(PIN_FAN_3, HIGH);
                    fanOn_3 = 1;
                    fanTimer_3 = canData[1];
                    prevFanTime_3 = millis();
                    break;
                default:
                    break;
                }
            }
        }
    }

    //------------------//
    //  UART/USB input  //
    //------------------//
    //
    //
    //-------------------------------------------------------//
    //                                                       //
    //      /////////    //\\        ////    //////////      //
    //    //             //  \\    //  //    //        //    //
    //    //             //    \\//    //    //        //    //
    //    //             //            //    //        //    //
    //    //             //            //    //        //    //
    //    //             //            //    //        //    //
    //      /////////    //            //    //////////      //
    //                                                       //
    //-------------------------------------------------------//
    if (Serial.available())
    {
        String input = Serial.readStringUntil('\n');
        Serial.println(input);

        input.trim();                    // Remove preceding and trailing whitespace
        std::vector<String> args = {}; // Initialize empty vector to hold separated arguments
        parseInput(input, args); // Separate `input` by commas and place into args vector
        args[0].toLowerCase();         // Make command case-insensitive
        String command = args[0];      // To make processing code more readable

        //--------//
        //  Misc  //
        //--------//
        /**/ if (command == "ping")
        {
            Serial.println("pong");
        }

        else if (command == "time")
        {
            Serial.println(millis());
        }

        else if (command == "led")
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

        //-----------//
        //  Sensors  //
        //-----------//

        //----------//
        //  Motors  //
        //----------//

        // Fan
        if (args[0] == "fans")  // Is looking for a command that looks like "fans,1,time"
        {
            Serial.println(args[0]);
            digitalWrite(PIN_FAN_1, args[1].toInt());
            digitalWrite(PIN_FAN_2, args[1].toInt());
            digitalWrite(PIN_FAN_3, args[1].toInt());
            fansOn = 1;
            fansTimer = args[2].toInt();
            prevFanTime = millis();
            Serial.println("Fans Activated");
        }

        else if (args[0] == "fan")  // Is looking for a command that looks like "fan,2,1,time"
        {
            Serial.println("Reached fan");
            switch (args[1].toInt())
            {
            case 1:
                digitalWrite(PIN_FAN_1, args[2].toInt());
                fanOn_1 = 1;
                fanTimer_1 = args[3].toInt();
                prevFanTime_1 = millis();
                break;
            case 2:
                digitalWrite(PIN_FAN_2, args[2].toInt());
                fanOn_2 = 1;
                fanTimer_2 = args[3].toInt();
                prevFanTime_2 = millis();
                break;
            case 3:
                digitalWrite(PIN_FAN_3, args[2].toInt());
                fanOn_3 = 1;
                fanTimer_3 = args[3].toInt();
                prevFanTime_3 = millis();
                break;
            default:
                break;
            }
            Serial.println("Fan Activated");
        }
        
        else if (args[0] == "servo")
        {
            switch (args[1].toInt())
            {
            case 1:
                servo1.write(args[2].toInt());
                break;
            case 2:
                servo2.write(args[2].toInt());
                break;
            case 3:
                servo3.write(args[2].toInt());
                break;
            default:
                break;
            }
        }

        else if (args[0] == "vib")
        {
            digitalWrite(PIN_VIBMOTOR, args[1].toInt());
        }

        else if (args[0] == "shutdown")
        {
            digitalWrite(13, LOW);
            digitalWrite(14, LOW);
            digitalWrite(15, LOW);
            digitalWrite(32, LOW);
            digitalWrite(33, LOW);
            digitalWrite(5, LOW);
            digitalWrite(12, LOW);
            digitalWrite(19, LOW);
            digitalWrite(21, LOW);
            digitalWrite(27, LOW);

            myLSS.reset();
            servo1.detach();
            servo2.detach();
            servo3.detach();
        }

        else if (args[0] == "lynx")
        {
          if (command != input)
          {
            if (args[1] == "rel")
            {
              myLSS.moveRelative(((args[2]).toInt()) * 10);
              Serial.println(myLSS.getPosition());
            }
            else if (args[1] == "zero")
            {
              myLSS.move(800);
              Serial.println("Full Retractig CITADEL arm");
            }
            else if (args[1] == "half")
            {
              myLSS.move(-8000);
              Serial.println("Setting arm to half extend");
            }
            else if (args[1] == "ext")
            { //
              myLSS.moveRelative(-1000);
              Serial.println("Extending CITADEL arm");
            }
            else if (args[1] == "ret")
            { //
              myLSS.moveRelative(1000);
              Serial.println("Retractig CITADEL arm");
            }
            else if (args[1] == "Reset")
            {
              myLSS.reset();
              Serial.println("Servo Reset");
            }
          }
        }
        else if (args[0] == "lss")
        {
            myLSS.wheel(args[1].toFloat());
        }
    }

    // if (MOTOR_UART.available()) {
    //     String input = MOTOR_UART.readStringUntil('\n');
    //     input.trim();
    //     COMMS_UART.println(input);
    // }
}

//------------------------------------------------------------------------------------------------//
//  Function definitions
//------------------------------------------------------------------------------------------------//
//
//
//----------------------------------------------------//
//                                                    //
//    //////////    //          //      //////////    //
//    //            //\\        //    //              //
//    //            //  \\      //    //              //
//    //////        //    \\    //    //              //
//    //            //      \\  //    //              //
//    //            //        \\//    //              //
//    //            //          //      //////////    //
//                                                    //
//----------------------------------------------------//.
