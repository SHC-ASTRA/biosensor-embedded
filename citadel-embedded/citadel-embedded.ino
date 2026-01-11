/**
 * @file main.cpp
 * @author Jack Schumacher (jackrschumacher@gmail.com)
 *
 */
#include <Arduino.h>
#include <ESP32Servo.h>
// #include <ASTRARevCAN.h>

// Remove to disable the boards inbuilt LED blinking
#define BLINK



void setup() {
  // put your setup code here, to run once:
  // Defined servos (3 for valves, 3 for distributors, 3 for chemicals)
  Servo valve1, valve2, valve3, distributor1, distrutor2, distributor3, chemical1, chemical2, chemical3;
  Serial.begin(9600); // May need to change this later
  pinMode(LED_BUILTIN, OUTPUT); // Enable the bult-in LED so that we can use it for status

}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) //Check if the serial is avaliable
    {
      // TODO: Create an input parser function, might be able to clean this whole block up
        String input = Serial.readStringUntil('\n');
        Serial.println(input);
        char command_Buffer
        input.toCharArray(command_Buffer, 50);
    }

}


