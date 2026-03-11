/**
 * @file Template_ArduinoIDE.cpp
 * @author your name (you@domain.com)
 * @brief description
 *
 */

//------------//
//  Includes  //
//------------//

#include <Arduino.h>  // Not required in Arduino IDE, but needed for PlatformIO projects
#include <vector>
#include <ESP32Servo.h>

//------------//
//  Settings  //
//------------//

// Comment out to disable LED blinking
#define BLINK

#define SERIAL_BAUD 115200

#define SERVO_PIN 23

//---------------------//
//  Component classes  //
//---------------------//

Servo servo;


//----------//
//  Timing  //
//----------//

uint32_t lastBlink = 0;
bool ledState = false;


//--------------//
//  Prototypes  //
//--------------//

void parseInput(const String input, std::vector<String>& args);
double map_d(double x, double in_min, double in_max, double out_min, double out_max);


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
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);

    pinMode(SERVO_PIN, OUTPUT);
    servo.attach(SERVO_PIN, 1000, 2000); // Attach servo to pin with min and max pulse widths (in microseconds)


    //------------------//
    //  Communications  //
    //------------------//

    Serial.begin(SERIAL_BAUD);


    //-----------//
    //  Sensors  //
    //-----------//


    //--------------------//
    //  Misc. Components  //
    //--------------------//
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
    if (millis() - lastBlink > 1000) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
    }
#endif


    //-------------//
    //  CAN Input  //
    //-------------//


    //------------------//
    //  UART/USB Input  //
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

        input.trim();                   // Remove preceding and trailing whitespace
        std::vector<String> args = {};  // Initialize empty vector to hold separated arguments
        parseInput(input, args);   // Separate `input` by commas and place into args vector
        args[0].toLowerCase();          // Make command case-insensitive
        String command = args[0];       // To make processing code more readable

        //--------//
        //  Misc  //
        //--------//
        // Always send 'command' as a PWM signal
        if (command == "ERR_NOINPUT") {
            servo.write(90);  // Default position
            Serial.println("Writing 90*");
        } else {
            int angle = int(map_d(command.toFloat(), 0, 1, 0, 180));
            servo.write(angle);
            Serial.print("Writing ");
            Serial.print(angle);
            Serial.println("*");
        }

        //-----------//
        //  Sensors  //
        //-----------//

        //----------//
        //  Motors  //
        //----------//
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

// Pulled from astra-embedded-lib
void parseInput(const String input, std::vector<String>& args) {
#define CMD_DELIM ','

    int lastIndex = -1;
    int index = -1;

    // Prevent MCU crash from attempting to access args[0]
    if (input.length() == 0) {
        args.push_back("ERR_NOINPUT");
        return;
    }

    unsigned count = 0;
    while (count++, count < 200) {
        lastIndex = index;
        index = input.indexOf(CMD_DELIM, lastIndex + 1);
        if (index == -1) {
            args.push_back(input.substring(lastIndex + 1));
            break;
        } else {
            args.push_back(input.substring(lastIndex + 1, index));
        }
    }

    // output is via vector<String>& args
}

double map_d(double x, double in_min, double in_max, double out_min, double out_max) {
    const double run = in_max - in_min;
    if (run == 0)
    {
	    return 0;  // in_min == in_max, error
    }
    const double rise = out_max - out_min;
    const double delta = x - in_min;
    return (delta * rise) / run + out_min;
}