/**
 * @file timer_events.ino
 * 
 * @author: Braidan Duffy <braidan.duffy@gmail.com>
 * @date: 30 November 2024
 * Modified on: [DATE_MODIFIED] by: [MODIFIER]
 * 
 * @brief A demonstration of using the TimerEvents class to do the basic Arduino
 * blink script
 * 
 * Depends on https://github.com/Legohead259/Timer-Events-Arduino being downloaded or cloned
 * into the Arduino/sketchbook/libraries directory.
 *
 * License: MIT
 * Disclaimer: At this time, this code is not tested on hardware. USE AT YOUR OWN RISK. 
 * Please report bugs to the author.
 * 
 * @version 0.1.0
 */

#include <Arduino.h>
#include <TimerEvents.h>

bool ledOn = false;

static void ledCallback() {
    ledOn = !ledOn;
    digitalWrite(LED_BUILTIN, ledOn);
}

void setup() {
    Serial.begin(9600);
    while(!Serial); // Wait for serial port to open

    Serial.println("Timer Event Demonstration!");

    pinMode(LED_BUILTIN, OUTPUT);

    TimerEvent blinkEvent("Blink Event", 1000, ledCallback, true);
    timerEvents.add(&blinkEvent);
    timerEvents.printTasking(&Serial);
}

void loop() {
    timerEvents.tasks();
}
