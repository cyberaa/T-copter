// Project Infinity - The UAV T-copter drone
// Code for RF transmitter
/*
Author  :  Pranjal Joshi
Date    :  30-4-2014
License :  GNU GPL v2 (Relased in public domain as open-source).
Disclaimer :
    This code may contain bugs as it is in development stage.
    Feel free to edit/improve/share.
*/
#include <PID_v1.h>
#include <RF24.h>
#include <SPI.h>

#define DEBUG 1 // set 1 to debug, 0 while regular use

// ---  Configurations   ---
#define PAYLOADSIZE 10
#define kpYaw 2.0
#define kpRoll 2.0
#define kpPitch 2.0

// ---  Hardware pin map  ---
#define throtPin A0
#define yawPin A1
#define pitchPin A2
#define rollPin A3
#define buttonPin 2
#define autopilotPin 3
#define altiholdPin 4
#define lightsPin 5
#define ledPin 8

// --- Global Variables  ---
uint8_t buttonState=0,autopilotState=0,altiholdState=0,lightsState=0,specialKeys=0;
uint8_t lastState[4] = {0,0,0,0};
uint8_t cnt[4] = {0,0,0,0};
uint8_t ccnt;
double KiThrottle;
double throt,opthrot;
double yaw,opyaw;
double pitch,oppitch;
double roll,oproll;
double calThrot=0,calYaw=0,calPitch=0,calRoll=0;
const uint64_t pipe = 0xF0F0F0F0E1LL;  // radio pipe
uint16_t radioFrame[PAYLOADSIZE/2];
uint16_t radioFrameChanged[sizeof(radioFrame)+1];

// --- Objects/Instances  ---
RF24 radio(9,10);

PID throttleJoystick(&throt,&opthrot,&calThrot,0,KiThrottle,0,DIRECT);

PID yaw1Joystick(&yaw,&opyaw,&calYaw,0,0.5,0,DIRECT);
PID yaw2Joystick(&yaw,&opyaw,&calYaw,0,0.5,0,REVERSE);

PID pitch1Joystick(&yaw,&opyaw,&calYaw,0,0.5,0,DIRECT);
PID pitch2Joystick(&yaw,&opyaw,&calYaw,0,0.5,0,REVERSE);

PID roll1Joystick(&yaw,&opyaw,&calYaw,0,0.5,0,DIRECT);
PID roll2Joystick(&yaw,&opyaw,&calYaw,0,0.5,0,REVERSE);


// --- Begining of main program routine  ---
void setup()
{
  #if DEBUG
    Serial.begin(9600);
  #endif
  pinMode(buttonPin,INPUT);
  pinMode(altiholdPin,INPUT);
  pinMode(autopilotPin,INPUT);
  pinMode(lightsPin,INPUT);
  pinMode(ledPin,OUTPUT);
  // --- enable internal pull up resistors of inputs ---
  digitalWrite(buttonPin,HIGH);
  digitalWrite(altiholdPin,HIGH);
  digitalWrite(autopilotPin,HIGH);
  digitalWrite(lightsPin,HIGH);
  // --- 1st time compensation  ---
  opyaw = 150;
  oproll = 150;
  oppitch = 150;
  for(ccnt=0;ccnt<sizeof(radioFrameChanged);ccnt++)
  {
    radioFrameChanged[ccnt] = 0;
  }
  callibrateJoysticks();
  initRadio();
  initPID();
}

void loop()
{
  memcpy(radioFrameChanged,radioFrame,sizeof(radioFrameChanged));
  readThrottle();
  readYaw();
  readPitch();
  readRoll();
  readButton(); 
  transmittRadio(); 
}

void initRadio()
{
  /*
  This function initialize nRF24l01 @ 250Kbps for higher range.
  Payload size is static & 10 bytes (i.e 5 integers)
  pipe is 64 bit address of module used for commuincation.
  pipe is similar to IP addr of computer.
  Power Amplifier (PA) is set to highest level --> Increased power consumption!
  */
  radio.begin();
  if(!(radio.setDataRate(RF24_250KBPS)))
    radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setRetries(5,10);
  radio.setPayloadSize(PAYLOADSIZE);
  radio.openWritingPipe(pipe);
}

void initPID()
{
  /*
  This initialize all PID controllers used to create flight control signals
  from analog joystick.
  Change output range as per requirement.
  */
  throttleJoystick.SetMode(AUTOMATIC);
  throttleJoystick.SetOutputLimits(0,1023);

  yaw1Joystick.SetMode(AUTOMATIC);
  yaw1Joystick.SetOutputLimits(100,150);
  yaw2Joystick.SetMode(AUTOMATIC);
  yaw2Joystick.SetOutputLimits(150,200);
  
  pitch1Joystick.SetMode(AUTOMATIC);
  pitch1Joystick.SetOutputLimits(100,150);
  pitch2Joystick.SetMode(AUTOMATIC);
  pitch2Joystick.SetOutputLimits(150,200);
  
  roll1Joystick.SetMode(AUTOMATIC);
  roll1Joystick.SetOutputLimits(100,150);
  roll2Joystick.SetMode(AUTOMATIC);
  roll2Joystick.SetOutputLimits(150,200);
}

void readThrottle()
{
  /*
  This function reads the analog joystick as throttle using mathematical
  Integration using PID library.
  The adaptive tunings are provided for higher accuracy on smaller change &
  faster action of large change in joystick position.
  */
  throt = analogRead(throtPin);
  if(throt < (calThrot - 10) || throt > (calThrot + 10))
  {
    // adaptive tunnings
    if(throt < (calThrot - 150) || throt > (calThrot + 150))
    {
      KiThrottle = 0.2;          // more sensitivity for minor change
      throttleJoystick.SetTunings(0,KiThrottle,0);
    }
    else
    {
      KiThrottle = 0.85;          // agressive for major change
      throttleJoystick.SetTunings(2,KiThrottle,0);
    }
    throttleJoystick.Compute();
    radioFrame[0] = opthrot;
  }
  #if DEBUG
    Serial.print(throt);
    Serial.print("\t\t");
    Serial.print(radioFrame[0]);
    Serial.print("\t\t");
  #endif
}

void readYaw()
{
  /*
  Reads yaw in discrete values.
  */
  yaw = analogRead(yawPin);
  if(yaw < (calYaw - 10))
  {
    yaw1Joystick.Compute();
    if(opyaw > 100)
      opyaw -= 50;
  }
  else if(yaw > (calYaw + 10))
  {
    yaw2Joystick.Compute();
  }
  else
  {
    opyaw = 150;
  }
  #if DEBUG
      Serial.print(yaw);
      Serial.print("\t\t");
      Serial.print(opyaw);
  #endif
  radioFrame[1] = opyaw;
}

void readPitch()
{
  /*
  Reads pitch in discrete values.
  */
  pitch = analogRead(pitchPin);
  if(pitch < (calPitch - 10))
  {
    pitch1Joystick.Compute();
    if(oppitch > 100)
      oppitch -= 50;
  }
  else if(pitch > (calPitch + 10))
  {
    pitch2Joystick.Compute();
  }
  else
  {
    oppitch = 150;
  }
  #if DEBUG
      Serial.print("\t\t");
      Serial.print(pitch);
      Serial.print("\t\t");
      Serial.print(oppitch);
      Serial.print("\t\t");
  #endif
  radioFrame[2] = oppitch;
}

void readRoll()
{
  /*
  Reads roll in discrete values.
  */
  roll = analogRead(rollPin);
  if(roll < (calRoll - 10))
  {
    roll1Joystick.Compute();
    if(oproll > 100)
      oproll -= 50;
  }
  else if(roll > (calRoll + 10))
  {
    roll2Joystick.Compute();
  }
  else
  {
    oproll = 150;
  }
  #if DEBUG
      Serial.print(roll);
      Serial.print("\t\t");
      Serial.print(oproll);
      Serial.print("\t\t");
  #endif
  radioFrame[3] = oproll;
}

void readButton()
{
  /*
  scan for all button i.e pressed or not.
  must be call in loop to update.
  */
  
  // random button -- function not specified
  buttonState = digitalRead(buttonPin);
  #if DEBUG
    Serial.print("\t");
    Serial.print(buttonState);
  #endif
  if(buttonState != lastState[0])
  {
    delay(1);
    if(buttonState == HIGH)
      cnt[0]++;
  }
  lastState[0] = buttonState;
  if(cnt[0] % 2 == 0)
  {
    specialKeys &= 0B11111110;
  }
  else
  {
    specialKeys |= 0B00000001;
  }
  
  //--- read autoPiolt button ---
  
  autopilotState = digitalRead(autopilotPin);
  #if DEBUG
    Serial.print("\t");
    Serial.print(autopilotState);
  #endif
  if(autopilotState != lastState[1])
  {
    delay(1);
    if(autopilotState == HIGH)
      cnt[1]++;
  }
  lastState[1] = autopilotState;
  if(cnt[1] % 2 == 0)
  {
    specialKeys &= 0B11111101;
  }
  else
  {
    specialKeys |= 0B00000010;
  }
  
  // --- read altihold button ---
  
  altiholdState = digitalRead(altiholdPin);
  #if DEBUG
    Serial.print("\t");
    Serial.print(altiholdState);
  #endif
  if(altiholdState != lastState[2])
  {
    delay(1);
    if(altiholdState == HIGH)
      cnt[2]++;
  }
  lastState[2] = altiholdState;
  if(cnt[2] % 2 == 0)
  {
    specialKeys &= 0B11111011;
  }
  else
  {
    specialKeys |= 0B00000100;
  }
  // --- read Light control button ---
  
  lightsState = digitalRead(lightsPin);
  #if DEBUG
    Serial.print("\t");
    Serial.println(lightsState);
  #endif
  if(lightsState != lastState[3])
  {
    delay(1);
    if(lightsState == HIGH)
      cnt[3]++;
  }
  lastState[3] = lightsState;
  if(cnt[3] % 2 == 0)
  {
    specialKeys &= 0B11110111;
  }
  else
  {
    specialKeys |= 0B00001000;
  }
  
  // compose frame
  radioFrame[4] = specialKeys;
}

void callibrateJoysticks()
{
  /*
  calibrate joystick pots on begining.
  eliminate need of static calibration.
  Improved performance.
  Must called once in setup.
  */
  for(ccnt=0;ccnt<10;ccnt++)
    calThrot += analogRead(throtPin);
  calThrot = (uint16_t)(calThrot/10);
  
  for(ccnt=0;ccnt<10;ccnt++)
    calYaw += analogRead(yawPin);
  calYaw = (uint16_t)(calYaw/10);
  
  for(ccnt=0;ccnt<10;ccnt++)
    calRoll += analogRead(rollPin);
  calRoll = (uint16_t)(calRoll/10);
  
  for(ccnt=0;ccnt<10;ccnt++)
    calPitch += analogRead(pitchPin);
  calPitch = (uint16_t)(calPitch/10);
  
  #if DEBUG
    Serial.print(F("Throttle Callibration\t:\t"));
    Serial.println(calThrot);
    Serial.print(F("Roll Callibration\t:\t"));
    Serial.println(calRoll);
    Serial.print(F("Pitch Callibration\t:\t"));
    Serial.println(calPitch);
    Serial.print(F("Yaw Callibration\t:\t"));
    Serial.println(calYaw);
    delay(3000);
  #endif
}

void transmittRadio()
{
  /*
  This function check for change in last readings & current reading.
  It transmitts data only when the last & current readings aren't same.
  This will save power & reduce unwanted signal transmission.
  */
  boolean tx = false;
  for(ccnt=0;ccnt<sizeof(radioFrameChanged);ccnt++) 
  {
    if(radioFrame[ccnt] != radioFrameChanged[ccnt]) 
      tx = true;
  }
  if(tx)
    radio.write(radioFrame,sizeof(radioFrame));
}