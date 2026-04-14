/**
 * @file main.cpp
 * @author Roald Schaum (rks0015@uah.edu)
 * @brief ASTRA Biosensor LIBS embedded code
 *
 */

#include <Wire.h>
#include <Arduino.h>

#include <cmath>
#include <thread>
#include <chrono>
#include <typeinfo>

#include "AstraMisc.h"
#include "AstraVicCAN.h"
#include <unilib/can_defs.hpp>

// Remove to disable the boards inbuilt LED blinking
#define BLINK
#define CAN_TX 16
#define CAN_RX 17

#define COMMS_UART Serial // To/from USB for debugging

using std::chrono::microseconds;
using std::chrono::steady_clock;
using std::chrono::time_point;
using std::this_thread::sleep_until;

bool ledState = false;

std::vector<double> ctrlValues = {};

const microseconds us_5 = microseconds(5);
time_point<steady_clock, steady_clock::duration> delay_1, delay_2;

#define CCDPixelCount 3694

// CCD Control
#define ShiftGate 30
#define ClearIntegrator 23
#define CCDOut 20
// TwoWire MasterClock;

void CCD(void* _);

static bool RunCCD;

static double pixels[CCDPixelCount] = {};

void setup()
{
    Serial.begin(SERIAL_BAUD);
    
    pinMode(ShiftGate, OUTPUT);
    pinMode(ClearIntegrator, OUTPUT);
    pinMode(CCDOut, OUTPUT);
    // MasterClock = TwoWire(24);

    if (ESP32Can.begin(TWAI_SPEED_1000KBPS, CAN_TX, CAN_RX))
        Serial.println("CAN bus started!");
    else
        Serial.println("CAN bus failed!");

    xTaskCreatePinnedToCore (
        CCD,     // Function to implement the task
        "CCDReader",   // Name of the task
        // https://dl.espressif.com/github_assets/espressif/xtensa-isa-doc/releases/download/latest/Xtensa.pdf
        // BSA : 16, Page 6
        // Frame return : 4
        // 64-bit stack push buffer : 8
        // 2x32-bit stack push buffer : 8
            16 + 4 + 8 + 8,      // Stack size in bytes
        malloc(CCDPixelCount * __SIZEOF_DOUBLE__),      // Task input parameter
        0,         // Priority of the task
        NULL,      // Task handle.
        0          // Core where the task should run
    );
}

void loop()
{
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
            Serial.println("pong");
        else if (command == "time")
            Serial.println(millis());
        else if (command == "led") // This command will not work when using boards that do not have a inbuilt
                                   // LED (i.e. the ESP32 Dev Module 1)
        {
            digitalWrite(LED_BUILTIN, !ledState);
            ledState = !ledState;
        }
        else if (args[0] == "can_relay_tovic")
            vicCAN.relayFromSerial(args);
        else if (args[0] == "can_relay_mode")
        {
            if (args[1] == "on")
                vicCAN.relayOn();
            else if (args[1] == "off")
                vicCAN.relayOff();
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

        // Process CAN commands


        switch (commandID)
        {
            case CMD_PING:
            {
                vicCAN.respond(1); // "pong"
                Serial.println("Received ping over CAN");
                break;
            }
            case CMD_FIRE_LIBS:
            {
                // The controller must send a unique ulong each time it intends
                // to fire. Otherwise, we skip the command   
                const double ctrl = canData[0];
                for(const double &pastValues : ctrlValues)
                {
                    if(ctrl == pastValues)
                    {
                        Serial.print("ERROR: Received duplicate LIBS fire key! Not firing the laser.");
                        break;
                    }
                }

                // Add supplied control key to list of used control keys
                ctrlValues.push_back(ctrl);

                // FIRE THE LASER!!!!!!!!!!!!!!!!!!!!


                // Read data on the CCD:
                // Only 3648 of the 3694 pixels are useful (as per datasheet)
                // Allocating it here so that the thread doesn't have to return it
                
                // Triggers the CCD thread
                RunCCD = true;
                
                break;
            }
            default:
                break;
        }
    }
}

void CCD(void* _)
{
    // Threads *have* to receive void*s. This just casts the interperetation to double* 
    double* pixels = static_cast<double*>(_);

    // Store this obj / ref in a register object
    register time_point<steady_clock, steady_clock::duration> delay_t;

    // Ensure count is stored as a register
    register ushort count = 0;

    while(true)
    {
        if(!RunCCD)
            sleep(1);
            continue;

        // Maximum clk rate of the ESP32 DOIT Devkit V1 is 240MHz -> 4.1667ns/instruction
        // The XTensa processor architecture has a 5 (or 7) stage pipeline
        // Therefore each instruction has an entry to completion delay of 20.8 to 29.1ns

        // asm should look something like
        // rn <- us_5                          | 
        // push rn2, call constructor, pop rn2 | one clock cycle between these
        // rn2 <- rn2 + rn                       | holds up the pipeline (sad!) 
        // delay_1 <- rn2                          |
        // delay_2 <- rn2 + rn                     | one clock cycle between these
        delay_t = steady_clock::now() + us_5;
        delay_1 = delay_t;
        delay_2 = delay_t + us_5;

        // Open the electronic shutter
        digitalWrite(ShiftGate, 1);
        digitalWrite(ClearIntegrator, 0);

        // Wait till the shift register propogates
        sleep_until(delay_1);

        digitalWrite(ShiftGate, 0);

        // Wait again
        sleep_until(delay_2);

        // Close the electronic shutter
        digitalWrite(ShiftGate, 1);
        digitalWrite(ClearIntegrator, 1);

        // Read the data collected
        for(; count < CCDPixelCount; count++)
        {
            delay_t = steady_clock::now() + us_5;
            delay_1 = delay_t;
            delay_2 = delay_t + us_5;

            // Sleep first because on first iter we just closed 
            // the shutter - has to propogate.
            sleep_until(delay_1);

            // Read the value from the ADC converter on the CCDOut pin,
            // write it to the corresponding location in the pixels array. 
            pixels[count] = digitalRead(CCDOut);
            digitalWrite(ShiftGate, 0);

            // Wait for the shift gate to propogate closed
            sleep_until(delay_2);

            // Open the shift gate, then wait for the ADC line to propagate
            // (next loop iteration)
            digitalWrite(ShiftGate, 1);
        }

        // Return the data collected
        count = 0;
        for(; count < CCDPixelCount; count++)
            vicCAN.respond(pixels[count]);
        return;
    }
}