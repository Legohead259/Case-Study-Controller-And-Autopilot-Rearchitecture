/**
 * button_inputs.ino
 * 
 * Author: Braidan Duffy <braidan.duffy@gmail.com>
 * Created on: 24 November 2024
 * Modified on: [DATE_MODIFIED] by: [MODIFIER]
 * 
 * This script is an example for encapsulating a button input using Arduino.
 * We start by define a button structure or object and the rest of the system variables.
 * We then define system callbacks that the buttons will enact when pressed.
 * In this example, the button will toggle an LED.
 * 
 * License: MIT
 * Disclaimer: At this time, this code is not tested on hardware. USE AT YOUR OWN RISK. 
 * Please report bugs to the author.
 * 
 * Version 0.1.0
 */

#define DEBOUNCE_TIME_MS 50 // Used to mitigate noisy switch inputs in software, rather than use a hardware lowpass filter
#define BUTTON_INPUT_PIN 7 // The Arduino pin connected to the button input

class Button {
    public:
    // Settable properties
    uint8_t pin;
    bool swDebounce = true; // Toggle if the button uses internal software debounce or not. Default: true
    bool activeState = LOW; // The button state in which we want to activate the functionality. E.g. LOW if the button is configured as an active low. Default: LOW
    void (*callback)(); // The action we want the button the do as a function

    void poll() {
        _reading = digitalRead(pin);
        if (swDebounce && !_debounce()) { // Enable and enact software debouncing
            _lastState = _reading; // Set last button state and exit
        }
        else if (_reading == activeState) { // If swDebounce is FALSE or the software debounce time has passed and the button is in the active state
            if (callback != nullptr) callback(); // execute the callback
            _lastState = _reading; // Set last button state and exit
        }
    }

    // Internal properties
    private:
    bool _reading;
    bool _currentState;
    bool _lastState;
    unsigned long _lastPressed; // ms
    unsigned long _debounceDelay; // ms

    bool _debounce() {
        if (_reading != _lastState) {
            _lastPressed = millis();
        }
        if (!_reading && (millis() - _lastPressed) > _debounceDelay) {
                if (_reading != _currentState) {
                    _currentState = _reading;
                if (_currentState == activeState) { // Supports activating callback for both active high and active low configurations.
                    return true;
                }
            }
        }
        return false;
    }
};

Button buttonLED;

bool ledState = LOW;

void buttonPressedCallback();

void setup() {
    pinMode(BUTTON_INPUT_PIN, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);

    // Configure buttonLED
    buttonLED.pin = BUTTON_INPUT_PIN;
    buttonLED.swDebounce = true;
    buttonLED.activeState = LOW;
    buttonLED.callback = &buttonPressedCallback;
}

void loop() {
    buttonLED.poll();
}

void buttonPressedCallback() {
    digitalWrite(LED_BUILTIN, !ledState);
}