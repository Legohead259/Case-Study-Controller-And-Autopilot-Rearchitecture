#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <LoRa.h>
#include <TimerEvents.h>

// Define pins
#define JOYSTICK_X_PIN 0 // Sideways
#define JOYSTICK_Y_PIN 1 // Forward

#define LOITER_BTN_PIN 2
#define CALIBRATION_BTN_PIN 4
#define CALIBRATION_GND 6

#define LORA_SS 5
#define LORA_RST -1
#define LORA_INT 7

// Define constants
#define DEBOUNCE_TIME_MS 50


// ============================
// === DEFINE BUTTON OBJECT ===
// ============================


class Button {
    public:
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


// ================================
// === DECLARE HELPER FUNCTIONS ===
// ================================


void sendHeartbeatMessage();
void sendJoystickXValueMessage();
void sendJoystickYValueMessage();
void handleLoiterButton();
void handleCalibrationButton();


// ====================================
// === INSTANTIATE GLOBAL VARIABLES ===
// ====================================


// Buttons
Button buttonLoiter;
Button buttonCalibration;

// TimerEvents
TimerEvent sendHeartbeatMessageEvent("Heartbeat Message Event", 2000, sendHeartbeatMessage, true);
TimerEvent sendJoystickXValueMessageEvent("Joystick X Message Event", 200, sendJoystickXValueMessage, true);
TimerEvent sendJoystickYValueMessageEvent("Joystick Y Message Event", 300, sendJoystickYValueMessage, true);
TimerEventsClass events;

// NeoPixel
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL);
const uint32_t COLOR_READY = pixel.Color(0, 255, 0);
const uint32_t COLOR_ERROR = pixel.Color(155, 0, 0);
const uint32_t COLOR_OFF = pixel.Color(0, 0, 0);

// Joystick inputs
int xvalue,t_xvalue=1500;
int yvalue,t_yvalue=15000;

bool lock=false;


// =============================================
// === DECLARE ARDUINO APPLICATION FUNCTIONS ===
// =============================================


void setup() {
    Serial.begin(9600);
    
    // HB: heartbeat;   LR: loiter   GU: gain up   GD:gain down  CC: calibrate  1100-1900 x value left and right    11000 to 19000 y value forwards and back
    // Initialize pins
    pinMode(LOITER_BTN_PIN,INPUT_PULLUP);
    pinMode(JOYSTICK_Y_PIN,INPUT);
    pinMode(JOYSTICK_X_PIN,INPUT);
    pinMode(CALIBRATION_BTN_PIN,INPUT_PULLUP);
    pinMode(CALIBRATION_GND,OUTPUT);

    digitalWrite(CALIBRATION_GND,LOW); // Set to LOW because...

    // Initialize Loiter Button
    buttonLoiter.pin = LOITER_BTN_PIN;
    buttonLoiter.swDebounce = true;
    buttonLoiter.activeState = LOW;
    buttonLoiter.callback = &handleLoiterButton;

    // Initialize Calibration Button
    buttonCalibration.pin = CALIBRATION_BTN_PIN;
    buttonCalibration.swDebounce = true;
    buttonCalibration.activeState = LOW;
    buttonCalibration.callback = &handleCalibrationButton;

    // Initialize NeoPixel
    pixel.begin();
    pixel.setBrightness(100);
    pixel.setPixelColor(0, COLOR_READY);
    pixel.show();

    // Initialize LoRa module
    LoRa.setPins(LORA_SS, LORA_RST, LORA_INT);
    
    if (!LoRa.begin(915E6)) { // Attempt to start LoRa module
        Serial.println("Starting LoRa failed!");
        while (1) { // Blink error code
            pixel.setPixelColor(0, pixel.Color(155, 0, 0));
            pixel.show();
            delay(500);
            pixel.setPixelColor(0, pixel.Color(0, 0, 0));
            pixel.show();
            delay(200);
        }
    }

    timerEvents.add(&sendHeartbeatMessageEvent);
    timerEvents.add(&sendJoystickXValueMessageEvent);
    timerEvents.add(&sendJoystickYValueMessageEvent);
    timerEvents.printTasking(&Serial); // Debug
}

void loop() {
  
    // Serial.println("LoopStart");

    // Check joystick input
    if(!lock){
        t_yvalue=ceil(round(float(analogRead(A1))/10)); //back and forth
        t_yvalue=map(t_yvalue,0,102,1100,1900)+8;
        
        t_xvalue=ceil(round(float(analogRead(A0))/10)); //left and right
        t_xvalue=map(t_xvalue,0,102,11000,19000);
        
        // Check if xvalue has changed and enable sendXValue event based on that
        xvalue != t_xvalue ? sendJoystickXValueMessageEvent.enable() : sendJoystickXValueMessageEvent.disable();

        // Check if yvalue has changed and enable sendYValue event based on that
        yvalue != t_yvalue ? sendJoystickYValueMessageEvent.enable() : sendJoystickYValueMessageEvent.disable();
    }
    
    timerEvents.tasks();
    buttonLoiter.poll();
    buttonCalibration.poll();

    //Serial.println("Loop End");
}


// ===============================
// === DEFINE HELPER FUNCTIONS ===
// ===============================


void sendHeartbeatMessage() {
    LoRa.beginPacket();
    LoRa.print("HB");
    LoRa.endPacket();
    Serial.println("Sending HB");
}

void sendJoystickXValueMessage() {
    xvalue = t_xvalue;
    LoRa.beginPacket();
    LoRa.print(xvalue);
    LoRa.endPacket();
    Serial.print("X: ");
    Serial.println(xvalue);
}

void sendJoystickYValueMessage() {
    yvalue = t_yvalue;
    LoRa.beginPacket();
    LoRa.print(yvalue);
    LoRa.endPacket();
    Serial.print("Y: ");
    Serial.println(yvalue);
}

void handleLoiterButton() {
    LoRa.beginPacket();
    LoRa.print("LT");
    LoRa.endPacket();
    Serial.println("Sending Loiter");
    //lock=!lock;
    //Serial.println("Locking down joystick");
}

void handleCalibrationButton() {
    LoRa.beginPacket();
    LoRa.print("CC");
    LoRa.endPacket();
    Serial.println("Sending Calibration Request");
}