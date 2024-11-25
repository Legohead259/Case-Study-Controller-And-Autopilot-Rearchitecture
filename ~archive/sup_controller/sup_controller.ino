#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <LoRa.h>

Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL);

int xvalue,t_xvalue=1500;
int yvalue,t_yvalue=15000;
long heartbeattimer=0;

//debounce variables
unsigned long lastDebounceTime_Loiter = 0;  // the last time the output pin was toggled
unsigned long lastDebounceTime_Lock = 0;
unsigned long lastDebounceTime_GU = 0;
unsigned long lastDebounceTime_GD = 0;
unsigned long lastDebounceTime_C = 0;
unsigned long debounceDelay_misc = 50;    // the debounce time; increase if the output
int buttonState_Loiter;            // the current reading from the input pin
int buttonState_Lock;
int buttonState_GU;
int buttonState_GD;
int buttonState_C;
int lastButtonState_Loiter = HIGH;
int lastButtonState_Lock = HIGH;
int lastButtonState_GU = HIGH;
int lastButtonState_GD = HIGH;
int lastButtonState_C = HIGH;
unsigned long XsendingTimer=0;
unsigned long YsendingTimer=0;
unsigned long CsendingTimer=0;


bool lock=false;

void setup() {
 Serial.begin(9600);
 //HB: heartbeat;   LR: loiter   GU: gain up   GD:gain down  CC: calibrate  1100-1900 x value left and right    11000 to 19000 y value forwards and back
  pinMode(2,INPUT_PULLUP); //loiter button
  pinMode(1,INPUT);   //joystick Y (Forwards)
  pinMode(2,INPUT);   //joystick X (sideways)
  pinMode(4,INPUT_PULLUP);  //calibration button
  pinMode(6,OUTPUT);  //calibration ground
  digitalWrite(6,LOW);
  

  
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0, 255, 0));
  pixels.show();

  LoRa.setPins(5,-1,7);  //SS RST  Go
  
  if (!LoRa.begin(915E6)) {
    Serial.println("Starting LoRa failed!");
    while (1)
    {
      pixels.setBrightness(100);
      pixels.setPixelColor(0, pixels.Color(155, 0, 0));
      pixels.show();
      delay(500);
      pixels.setBrightness(0);
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
      delay(200);
    }
  }

  
}

void loop() {
  
 // Serial.println("LoopStart");

  //important heartbeat to signal connection
  
  if(millis()-heartbeattimer>2000)  
  {
    LoRa.beginPacket();
    LoRa.print("HB");
    LoRa.endPacket();
    heartbeattimer=millis();
    Serial.println("Sending HB");
  }

  //joystick
  if(!lock){
    t_yvalue=ceil(round(float(analogRead(A1))/10)); //back and forth
    t_yvalue=map(t_yvalue,0,102,1100,1900)+8;
    
    t_xvalue=ceil(round(float(analogRead(A0))/10)); //left and right
    t_xvalue=map(t_xvalue,0,102,11000,19000);
    
    
    if(xvalue!=t_xvalue && millis()-XsendingTimer>200)
    {
      xvalue=t_xvalue;
      LoRa.beginPacket();
      LoRa.print(xvalue);
      LoRa.endPacket();
      Serial.print("X: ");
      Serial.println(xvalue);
      XsendingTimer=millis();
    }
    if(yvalue!=t_yvalue && millis()-XsendingTimer>300)
    {
      yvalue=t_yvalue;
      LoRa.beginPacket();
      LoRa.print(yvalue);
      LoRa.endPacket();
      Serial.print("Y: ");
      Serial.println(yvalue);
      YsendingTimer=millis();
    }
  }
  //Debounce Buttons
  
  //Loiter
  int reading = digitalRead(2);
  
  if (reading != lastButtonState_Loiter) {
    lastDebounceTime_Loiter = millis();
  }
  if ((millis() - lastDebounceTime_Loiter) > debounceDelay_misc) {
    if (reading != buttonState_Loiter) {
      buttonState_Loiter = reading;
      if (buttonState_Loiter == LOW) {
        LoRa.beginPacket();
        LoRa.print("LT");
        LoRa.endPacket();
        Serial.println("Sending Loiter");
        //lock=!lock;
        //Serial.println("Locking down joystick");
      }
    }
  }
  lastButtonState_Loiter = reading;

  //Calibrate
  reading = digitalRead(4);
  if (reading != lastButtonState_C) {
    lastDebounceTime_C = millis();
  }
  if ((millis() - lastDebounceTime_C) > debounceDelay_misc) {
    if (reading != buttonState_C) {
      buttonState_C = reading;
      if (buttonState_C == LOW) {
        LoRa.beginPacket();
        LoRa.print("CC");
        LoRa.endPacket();
        Serial.println("Sending Calibration Request");
      }
    }
  }
  lastButtonState_C = reading;
  
  
  //Serial.println("Loop End");
 
  
}
