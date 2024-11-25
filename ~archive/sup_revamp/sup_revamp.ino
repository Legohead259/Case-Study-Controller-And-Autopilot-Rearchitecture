#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_NeoPixel.h>
#include <QMC5883LCompass.h>
#include <Adafruit_SSD1327.h>
#include <TinyGPSPlus.h>
#include <Servo.h>

#define OLED_RESET -1


// Displa
Adafruit_SSD1327 display(128, 128, &Wire, OLED_RESET, 1000000);
Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL);

//GPS
static const uint32_t GPSBaud = 9600;
TinyGPSPlus gps;

float hold_lat,hold_lon;
long _lat,_lon;
float Ftarget,Starget=90;
float Fcurrent,Scurrent=90;



//Compass
QMC5883LCompass compass;

float heading;
int _dir=0;
bool lock=false;

//servo
const int FWGain=1;
const float SWGain=1;
const int sdeadband=0;
const int fdeadband=0;
const int distance=30;
Servo Forwards, Sideways;
float gain=1;

//misc
bool displayTurning=false;
bool displayThrottle=false;

//voltage sensor
int voltageRemaining=0;
unsigned long voltageDelay=0;
int cumulativeVoltage=0;
int voltageCounter=0;

//remote

unsigned long switchDelay=0;
unsigned long switchValue=0;
String message="";

void setup() {
  while(!Serial && !Serial1);
  Serial.begin(115200);
  Serial1.begin(GPSBaud);

  pixels.begin();
  pixels.setBrightness(100);
  pixels.setPixelColor(0, pixels.Color(155, 155, 155));
  pixels.show();
  
  LoRa.setPins(0,-1,1); //set pins ss, reset, gi0
  
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

  
  pinMode(A1, INPUT);
  
  Forwards.attach(2);   //FWD is 2
  Sideways.attach(3);
  Forwards.write(90);
  Sideways.write(90);
  
  delay(1000);
  //Display stuff
  if ( ! display.begin(0x3D) ) {
     Serial.println("Unable to initialize OLED");
     while (1) yield();
  }
  delay(1000);
  display.clearDisplay();
  display.setRotation(3);
  display.setTextColor(SSD1327_WHITE);
  display.setCursor(0,0);
  display.println("Lat:");
  display.println("Lon:");
  display.println("Fix:");
  display.display();


  
  //Compass stuff
   compass.init();

 
}

void loop() {
  

  
  display.display();
  while (Serial1.available() > 0)
    gps.encode(Serial1.read());

    
  compass.read();
  heading=compass.getAzimuth()+180;
  _lon=gps.location.lng()*1000000;
  _lat=gps.location.lat()*1000000;


  cumulativeVoltage=cumulativeVoltage+int((analogRead(A1)-6970)/7.7);//7740 is 12.4 and 6970 is 11.5 that is a range of 770 so 0-100 is change of 7.7
  voltageCounter=voltageCounter+1;
  
  

   switchValue=0;
   message="";
   int packetSize = LoRa.parsePacket();
    if (packetSize) {
    // received a packet
      Serial.print("Received packet '");
 
    // read packet
      while (LoRa.available()) {
       message+=(char)LoRa.read();
      }

    // print RSSI of packet
      Serial.print("' with RSSI ");
      Serial.println(LoRa.packetRssi());
    }
  
  
  
  
  if (message.equals("HB"))
  {
    switchDelay=millis();
  }
  
  
  if (message.equals("LR")) //LOITER
  {
      lock=!lock;
      if(lock)
        {
          hold_lon=gps.location.lng()*1000000;
          hold_lat=gps.location.lat()*1000000;
          delay(500);    
        }else
        {
          
          Starget=90+sdeadband;
          Ftarget=90+fdeadband;
        }
      
  }

  if(millis()-switchDelay>5000) //lost contact with controller
  {
    lock=false;
    
    Starget=90+sdeadband;
    Ftarget=90+fdeadband;
    pixels.setPixelColor(0, pixels.Color(155, 155, 0));  //lost ocntact color color
    pixels.show();
  }else
  {
    pixels.setPixelColor(0, pixels.Color(0, 0, 155)); //healthy color
    pixels.show();
  }

  if (message.equals("GU")) //GAIN UP
  {
    gain=gain+0.1;
  }
  
  if (message.equals("GD")) //GAIN down
  {
    gain=gain-0.1;
    if(gain<1)
      gain=1;
  }

  if (message.equals("CC")) //calibrate
  {
    calibrateCompass();
  }

  if (message[0]=='1')
  {
    switchValue=message.toInt();
  }
  
  if(switchValue>100) //remote control
  {
    if(lock && switchValue<10000 && (switchValue>1600 || switchValue<1400))
      lock=false;
    if(lock && switchValue>10000 && (switchValue>16000 || switchValue<14000))
      lock=false;
      
    if(switchValue<10000 && !lock)
      {
        switchValue=map(switchValue,1100,1900,128,52);
        Ftarget=switchValue;
      }
    if(switchValue>10000 && !lock)
      {
        switchValue=map(switchValue,11000,19000,52,128);
        Starget=switchValue;
        
      }
  }
  

  //target PWM to current
  if(Ftarget<Fcurrent)
    {
      Fcurrent=Fcurrent-1;
    }
  if(Ftarget>Fcurrent)
    {
      Fcurrent=Fcurrent+1;
    }
 if(Starget<Scurrent)
    {
      Scurrent=Scurrent-1;
    }
  if(Starget>Scurrent)
    {
      Scurrent=Scurrent+1;
    }
  
  displayThrottle=false;
  displayTurning=false;
  
  //code for lock behaviour
  if(lock)
  {
    //if button pressed again, lock=false; turn off motors; return;
    if(holdDistance()>distance) //when out of range
    {
      //turn to hold position and compensate for forwards and backwards
      if(turnAngle()>10)  //while heading is not reached
      {
        displayTurning=true;
        displayThrottle=false;
        
        if(90+SWGain*turnAngle()*turnDirection()*gain<52)
          Starget=52;
        if(90+SWGain*turnAngle()*turnDirection()*gain>128)
          Starget=128;
        if(90+SWGain*turnAngle()*turnDirection()*gain>=52 && 90+SWGain*turnAngle()*turnDirection()*gain<=128)
          Starget=90+SWGain*turnAngle()*turnDirection()*gain;
          
        Sideways.write(Scurrent);
      }else //if heading is good, go forwards/backwards
      {
        displayTurning=false;
        displayThrottle=true;
        if(90+FWGain*holdDistance()*_dir*gain<52)
          Ftarget=52;
        if(90+FWGain*holdDistance()*_dir*gain>128)
          Ftarget=128;
        if(90+FWGain*holdDistance()*_dir*gain>=52 && 90+FWGain*holdDistance()*_dir*gain<=128)
          Ftarget=90+FWGain*holdDistance()*_dir*gain;

        Forwards.write(Fcurrent);
      }
    }else
    {
      //drift
     
      Starget=90;
      Ftarget=90;
      displayThrottle=false;
      displayTurning=false;
    }
  }else
  {
    Forwards.write(Fcurrent);
    Sideways.write(Scurrent); 
  }

  updateDisplay();
}



void updateDisplay()
{
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Lat:");
  display.print(_lat);
  display.println();
  display.print("Lon:");
  display.print(_lon);
  display.println();
  display.print("Satellites: ");
  //printInt(gps.satellites.value(), gps.satellites.isValid(), 5);
  display.print(gps.satellites.value());
  display.println();
  display.print("Heading: ");
  display.println(heading);
  display.print("Battery: ");
  display.print(voltageRemaining);
  display.print("%");
  
  display.drawCircle(64,78,25,255);
  display.drawLine(64,78,64,53,255);
  
  if(lock)
  {
    
    
    display.println("LOITER MODE!");
    display.setCursor(0,100);
    display.print("D:");
    display.print(holdDistance());
    display.setCursor(80,100);
    display.print("G:");
    display.print(gain);
    display.setCursor(0,120);
    display.print("H:");
    display.print(targetHeading());
    display.print("A:");
    display.print(turnAngle());
    display.print(",");
    display.print(turnDirection());
    
    display.drawLine(64,78,64+25*cos((targetHeading()-heading+90)*3.14/180),78-25*sin((targetHeading()-heading+90)*3.14/180),255);
    
    if((holdDistance()>distance)){
     if(displayThrottle)
      if(_dir==1)
        {
          display.setCursor(64,45);
          display.println("/\\");
        } 
        else
        {
          display.setCursor(64,113);
          display.println("\\/");
        } 
        
      if(displayTurning)
        if(turnDirection()==1)
        {
          display.setCursor(25,78);
          display.println("<");
        } 
        else
        {
          display.setCursor(103,78);
          display.println(">");
        } 
    } 
  }
  

}

long holdDistance()
{
 return (sqrt(sq(_lon-hold_lon)+sq(_lat-hold_lat)));
}
 
float targetHeading()   // sensor points at 0 when lat is up and lon is right
{
  float h;
  
  if(hold_lat>_lat && hold_lon>_lon)  //point is in first quadrant 
    h=atan((hold_lat-_lat)/(hold_lon-_lon))*180/3.1415;
  
  if(hold_lat>_lat && hold_lon<_lon)  //point is in second quadrant
    h=90+atan(abs((hold_lon-_lon)/(hold_lat-_lat)))*180/3.1415;

  if(hold_lat<_lat && hold_lon<_lon)  //point is in third quadrant
    h=180+atan(abs((hold_lat-_lat)/(hold_lon-_lon)))*180/3.1415;

  if(hold_lat<_lat && hold_lon>_lon)  //point is in fourth quadrant
    h=270+atan(abs((hold_lon-_lon)/(hold_lat-_lat)))*180/3.1415;
  
  if(hold_lon>_lon && hold_lat==_lat)  //point is at 0 degrees
    h=0;
  if(hold_lon==_lon && hold_lat>_lat)  //point is at 90 degrees
    h=90;
  if(hold_lat==_lat && hold_lon<_lon)  //point is at 180 degrees
    h=180;
  if(hold_lon==_lon && hold_lat<_lat)  //point is at 90 degrees
    h=270;
    
  return h;
}

float turnAngle()
{
  float degree=(int(targetHeading())-int(heading)+180)%360-180;
  if(abs(degree)>90) //facing away
  {
    _dir=-1;
    return (abs(180-degree));
  }
  else //facing towards
  {
    _dir=1;
    return(abs(degree));
  }
}

int turnDirection()
{
  int turndir=0;
  if(abs((int(targetHeading())-int(heading)+360)%360) <180)
     turndir=1;
  else
     turndir=-1;
     
  if (abs( ( (int(targetHeading())-int(heading)+180) %360-180)) >90) //invert if we are facing backwards
    turndir=turndir*-1;

  return(turndir);
}


static void calibrateCompass(){
  compass.init();
  display.clearDisplay();
  display.println("COMPASS CALIBRATION. When prompted, move the compass in all directions!");
  display.println("Calibration will begin in 10 seconds.");
  display.display();
  delay(5000);
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("CALIBRATING. Keep moving your sensor...");
  display.display();
  compass.calibrate();
  display.clearDisplay();
  display.println("DONE!!");
  display.display();
  delay(5000);
}
