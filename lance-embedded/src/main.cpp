/**
 * @file main.cpp
 * @brief Controls LANCE's linear actuators, stepper motors, SparkMax motors, and laser
 *
 */


//------------//
//  Includes  //
//------------//

#include <Adafruit_SHT31.h>
#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>

#include "AstraMisc.h"
#include "AstraMotors.h"
#include "AstraVicCAN.h"
#include "LancePins.h"


//------------//
//  Settings  //
//------------//

#define BLINK

#define STEPPER_STEPS_PER_REV 200
#define STEPPER_RPM 60

#define DRILL_MOTOR_ID 5


//---------------------//
//  Component classes  //
//---------------------//

AstraMotors drillMotor(DRILL_MOTOR_ID, false, 1);

Adafruit_SHT31 sht30 = Adafruit_SHT31();
bool shtAvailable = false;


//----------//
//  Timing  //
//----------//

Timer ledBlink;
Timer voltRead;
Timer shtRead;
Timer motorAccel;
Timer motorFeedback;
Timer versionFeedback;

bool ledState = false;


//--------------//
//  Prototypes  //
//--------------//

void setLinac(uint8_t linacId, float duty);
void allStop();

void heartbeatTask(void* pvParameters) {
    while (true) {
        CAN_sendHeartbeat(DRILL_MOTOR_ID);
        delay(10);
    }
}


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
void setup() {
    //--------//
    //  Pins  //
    //--------//

    pinMode(LED_BUILTIN, OUTPUT);

    // Linear Actuators
    pinMode(PIN_LINAC_LARGE_FIN, OUTPUT);
    pinMode(PIN_LINAC_LARGE_RIN, OUTPUT);
    pinMode(PIN_LINAC_SMALL_FIN, OUTPUT);
    pinMode(PIN_LINAC_SMALL_RIN, OUTPUT);
    digitalWrite(PIN_LINAC_LARGE_FIN, LOW);
    digitalWrite(PIN_LINAC_LARGE_RIN, LOW);
    digitalWrite(PIN_LINAC_SMALL_FIN, LOW);
    digitalWrite(PIN_LINAC_SMALL_RIN, LOW);


    //------------------//
    //  Communications  //
    //------------------//

    Serial.begin(SERIAL_BAUD);

    if (ESP32Can.begin(TWAI_SPEED_1000KBPS, PIN_CAN_TX, PIN_CAN_RX))
        Serial.println("CAN bus started!");
    else
        Serial.println("CAN bus failed!");


    //-----------//
    //  Sensors  //
    //-----------//

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (sht30.begin(0x44)) {
        shtAvailable = true;
        Serial.println("SHT30 found");
    } else {
        Serial.println("SHT30 not found");
    }


    //--------------------//
    //  Misc. Components  //
    //--------------------//


    // Timers
    ledBlink.interval = 1000;
    voltRead.interval = 1000;
    shtRead.interval = 2000;
    motorAccel.interval = 50;
    motorFeedback.interval = 500;
    versionFeedback.interval = 5000;

    // Heartbeat task for SparkMAX (must send every 25ms)
    xTaskCreatePinnedToCore(heartbeatTask, "heartbeat", 1000, NULL, 0, NULL, 0);

    // Configure SparkMAX status frame periods
    drillMotor.setSlowStatusPeriods();

    Serial.println("LANCE setup complete");
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
void loop() {
    //----------//
    //  Timers  //
    //----------//

#ifdef BLINK
    if (millis() - ledBlink.lastMillis >= ledBlink.interval) {
        ledBlink.lastMillis = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
#endif

    if (millis() - voltRead.lastMillis >= voltRead.interval) {
        voltRead.lastMillis = millis();
        float vBatt = convertADC(analogRead(PIN_ADC_VBATT), 10, 2.21);
        float v12 = convertADC(analogRead(PIN_ADC_12V), 10, 3.32);
        float v5 = convertADC(analogRead(PIN_ADC_5V), 10, 10);

        vicCAN.send(CMD_POWER_VOLTAGE, (int16_t)(vBatt * 100), (int16_t)(v12 * 100), (int16_t)(v5 * 100));
    }

    if (shtAvailable && millis() - shtRead.lastMillis >= shtRead.interval) {
        shtRead.lastMillis = millis();
        float temp = sht30.readTemperature();
        float hum = sht30.readHumidity();
        if (!isnan(temp) && !isnan(hum)) {
            vicCAN.send(CMD_SHT_TEMP_HUM, temp, hum);
        }
    }

    // Motor acceleration (smooth duty cycle ramping)
    if (millis() - motorAccel.lastMillis >= motorAccel.interval) {
        motorAccel.lastMillis = millis();
        drillMotor.accelerate();
    }

    // Send motor feedback over VicCAN
    if (millis() - motorFeedback.lastMillis >= motorFeedback.interval) {
        motorFeedback.lastMillis = millis();
        if (millis() - drillMotor.status1.timestamp < 500) {
            vicCAN.send(CMD_REVMOTOR_FEEDBACK, drillMotor.getID(), drillMotor.status1.motorTemperature * 10,
                        drillMotor.status1.busVoltage * 10, drillMotor.status1.outputCurrent * 10);
        }
        if (millis() - drillMotor.status1.timestamp < 500 && millis() - drillMotor.status2.timestamp < 500) {
            vicCAN.send(CMD_REV_POS_VEL_FEEDBACK, drillMotor.getID(), drillMotor.status2.sensorPosition,
                        drillMotor.status1.sensorVelocity);
        }
    }

    if (millis() - versionFeedback.lastMillis >= versionFeedback.interval) {
        versionFeedback.lastMillis = millis();
        SEND_VERSION_INFO
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

    CanFrame rxFrame;
    bool isREV;
    if (vicCAN.readCan(&isREV, &rxFrame)) {
        const uint8_t commandID = vicCAN.getCmdId();
        static std::vector<double> canData;
        vicCAN.parseData(canData);

        // General

        if (commandID == CMD_PING) {
            vicCAN.respond(1);
        }

        else if (commandID == CMD_TIME) {
            vicCAN.respond(static_cast<double>(millis()));
        }

        else if (commandID == CMD_B_LED) {
            if (canData.size() == 1) {
                digitalWrite(LED_BUILTIN, static_cast<int>(canData[0]));
            }
        }

        else if (commandID == CMD_ALL_STOP) {
            allStop();
        }

        else if (commandID == CMD_VERSION_COMMIT || commandID == CMD_VERSION_BUILD) {
            SEND_VERSION_INFO
        }

        // Misc Physical Control

        else if (commandID == CMD_LANCE_LINEAR_AC) {
            // canData[0] = linac ID (1 or 2), canData[1] = duty (-1.0 to 1.0)
            if (canData.size() == 2) {
                setLinac(static_cast<uint8_t>(canData[0]), static_cast<float>(canData[1]));
            }
        }

        else if (commandID == CMD_REV_SET_DUTY) {
            // canData[0] = duty (-1.0 to 1.0) for drill SparkMax
            if (canData.size() == 1) {
                drillMotor.setDuty(static_cast<float>(canData[0]));
            }
        }

    } else if (isREV) {
        // Parse REV SparkMAX status frames
        uint8_t deviceId = rxFrame.identifier & 0x3F;
        uint32_t apiId = (rxFrame.identifier >> 6) & 0x3FF;

        if (deviceId == drillMotor.getID() && (apiId & 0x60) == 0x60) {
            drillMotor.parseStatus(apiId, rxFrame.data);
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
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        Serial.println(input);

        input.trim();
        std::vector<String> args = {};
        parseInput(input, args);
        args[0].toLowerCase();
        String command = args[0];

        //--------//
        //  Misc  //
        //--------//
        if (command == "ping") {
            Serial.println("pong");
        }

        else if (command == "time") {
            Serial.println(millis());
        }

        else if (command == "led") {
            ledState = !ledState;
            digitalWrite(LED_BUILTIN, ledState);
        }

        else if (command == "can_relay_tovic") {
            vicCAN.relayFromSerial(args);
        }

        else if (command == "can_relay_mode") {
            if (args.size() > 1) {
                if (args[1] == "on")
                    vicCAN.relayOn();
                else if (args[1] == "off")
                    vicCAN.relayOff();
            }
        }

        //----------//
        //  Motors  //
        //----------//

        // Linear Actuators: "linac,<id>,<duty>"  duty: -1.0 to 1.0
        else if (command == "linac") {
            if (args.size() >= 3) {
                setLinac(args[1].toInt(), args[2].toFloat());
            }
        }

        // Drill SparkMax: "drill,<duty>"  duty: -1.0 to 1.0
        else if (command == "drill") {
            if (args.size() >= 2) {
                drillMotor.setDuty(args[1].toFloat());
            }
        }


        //-----------//
        //  Sensors  //
        //-----------//

        // SHT30: "sht"
        else if (command == "sht") {
            if (shtAvailable) {
                float temp = sht30.readTemperature();
                float hum = sht30.readHumidity();
                Serial.print("Temp: ");
                Serial.print(temp);
                Serial.print(" C, Hum: ");
                Serial.print(hum);
                Serial.println(" %");
            } else {
                Serial.println("SHT30 not available");
            }
        }

        // Emergency stop
        else if (command == "stop") {
            allStop();
            Serial.println("All stopped");
        }
    }
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
//----------------------------------------------------//

void setLinac(uint8_t linacId, float duty) {
    uint8_t pinFin, pinRin;

    if (linacId == 1) {
        pinFin = PIN_LINAC_LARGE_FIN;
        pinRin = PIN_LINAC_LARGE_RIN;
    } else if (linacId == 2) {
        pinFin = PIN_LINAC_SMALL_FIN;
        pinRin = PIN_LINAC_SMALL_RIN;
    } else {
        return;
    }

    duty = constrain(duty, -1.0f, 1.0f);
    uint8_t pwm = static_cast<uint8_t>(abs(duty) * 255);

    if (duty < 0) {  // Extend
        analogWrite(pinFin, pwm);
        analogWrite(pinRin, 0);
    } else if (duty > 0) {  // Retract
        analogWrite(pinFin, 0);
        analogWrite(pinRin, pwm);
    } else {  // Stop
        analogWrite(pinFin, 0);
        analogWrite(pinRin, 0);
    }
}

void allStop() {
    // Stop linear actuators
    setLinac(1, 0);
    setLinac(2, 0);

    // Stop drill motor via CAN
    drillMotor.stop();
}
