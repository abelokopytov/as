// ######################################################
#define VERSION "as1_32 16-bit"

// Cellophan filter 6 layers
// Thick common wire
// 16-bit

//for LCD 20x4 I2C
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
// declare the lcd object
hd44780_I2Cexp lcd; // auto locate and autoconfig interface pins
// tell the hd44780 sketch the lcd object has been declared
#define HD44780_LCDOBJECT
#define LCD_COLS 20
#define LCD_ROWS 4

//for DS18B20 temperature sensor
#include <OneWire.h> 
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 12 // pin 12
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature dtemp(&oneWire);

//https://github.com/j-bellavance/CommonBusEncoders
#include <CommonBusEncoders.h>
//encoders(BusA, BusB, BusSwitches, Number of encoders)
CommonBusEncoders encoders(A0, A1, A2, 3);

#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
SoftwareSerial mySoftwareSerial(8, 6); // RX, TX  - to communicate with DFPlayerMini
DFRobotDFPlayerMini myDFPlayer;

// from https://github.com/jfturcot/SimpleTimer
#include <SimpleTimer.h>
// There must be one global SimpleTimer object.
SimpleTimer timer;
int timerId;
int timerInterval = 1500; // 1,5 sec
bool autoChangePhase = true;

// Encoder 1
float    rgRatio;
float    rgStepSmall  = 0.001;
float    rgStepMedium = 0.01;
float    rgStepBig    = 0.1;
float    rgStep  = rgStepSmall;

// Encoder 2
unsigned int stepAmberSmall  =  16;
unsigned int stepAmberMedium =  64;
unsigned int stepAmberBig    = 256;
unsigned int stepAmber = stepAmberSmall;

//Encoder 3
float    stepLum = 1.0;

//const unsigned int maxValue =  1023; //10-bit resolution
const unsigned long maxValue =  65535; //16-bit resolution

//Pins
int pinGreen = 10;
int pinRed   =  9;
int pinAmber =  11;
int pinBlue  =  5;

unsigned long valueRed = 0;
unsigned long valueGreen = 0;
unsigned long valueAmber = 0;
unsigned long valueBlue = 0;

unsigned long maxValueRed;
unsigned long maxValueGreen = maxValue; //65535


//AS1 with SB1+cellophan filter 6 layers
// from spectra measurements 17.09.2024
// max Green Luminance 35.87 cd/m2
// max Red   Luminance 155.1 cd/m2
float maxLumGreen = 35.9; // boundary value -- cd/m2
float maxLumRed = 155.1;  // not used -- cd/m2

// For Luminance = 20 cd/m2 Red and Green values were defined by spectral measurements
float valueLum = 20.0;
// Measurements 2025.02.26 
//unsigned long valueGreenLum20 = 34500;
//unsigned long valueRedLum20 = 7450;
//unsigned long valueAmberLum20 = 11800;

// Measurements 2025.06.20 T=25.0
unsigned long valueGreenLum20 = 39200; //Yxy: 20.033197 0.355382 0.631694
unsigned long valueRedLum20 = 7810;    //Yxy: 20.051561 0.691100 0.308226
//unsigned long valueAmberLum20 = 11500; //Yxy: 20.022201 0.563441 0.435640
// Measurements 2025.06.23
// G=39200 Y=20.83 T=24.0; R=7810 Y=20.51 T=24.0; A=11500 Y=18.71 T=23.0
// increase A
//unsigned long valueAmberLum20 = 12500; //Yxy: 20.453128 0.561619 0.436786
// Measurements 2025.06.25
// G=39200 Y: 20.601455 T=24.4; R=7810 Y: 20.187054 T=24.6; A:12500 Y=18.851710 T=24.3
// increase A
unsigned long valueAmberLum20 = 13300; //Yxy: 20.872399 0.562126 0.435786 T=24.3


bool flagShowFlipFlop = true;

int phase = 0;

float fTemp; // Temperature

int mode = 0; // mode of operation == screen

//######################################
// Configure digital pins 9, 10, 11 as 10-bit PWM outputs. Freq = 976Hz
void setupPWM10() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) /* non-inverting PWM */
        | _BV(WGM11) | _BV(WGM10);                 /* mode 3: Phase correct, 10-bit */

  TCCR1B = _BV(CS11); //prescaler=8

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1) /* non-inverting PWM */
        | _BV(WGM31) | _BV(WGM30);                 /* mode 3: Phase correct, 10-bit */
  TCCR3B = _BV(CS31); //prescaler=8
}
//######################################
// Configure digital pins 9, 10, 11, and 5 as 16-bit PWM outputs. Freq = 244 Hz
void setupPWM16() { 
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7);
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) /* non-inverting PWM */
        | _BV(WGM11);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); //prescaler=1
  ICR1 = 0xFFFF;

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1) /* non-inverting PWM */
        | _BV(WGM31);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS30); //prescaler=1
  ICR3 = 0xFFFF;
}
//######################################
/* 16-bit version of analogWrite(). Works only on pins 9, 10, 11, 5 */
void analogWriteHR(int pin, unsigned int val) {
  switch (pin) {
    case  9: OCR1A = val; break; //Red
    case 10: OCR1B = val; break; //Green
    case 11: OCR1C = val; break; //Amber
    case  5: OCR3A = val; break; //Blue
  }
}
//######################################
String numToStrLen(unsigned int n, int len) {
  String result = "";
  for (int pos=1; pos < len; pos++) {
    if (n < pow(10, pos)) result += "0";
  }
  result += n;
  return result;
}
//####################################################################
void setup() {
    Serial.begin(115200);
    mySoftwareSerial.begin(9600); // for myDFPlayer
    delay(5000);
  
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(String(VERSION));
    delay(5000);
    lcd.clear();

    delay(5000);
    while (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
        Serial.println(F("Unable to begin:"));
        Serial.println(F("1.Please recheck the connection!"));
        Serial.println(F("2.Please insert the SD card!"));
        delay(5000); // Code to compatible with ESP8266 watch dog.
    }
    delay(5000);
    myDFPlayer.volume(20);  //Set volume value. From 0 to 30

    dtemp.begin();
    dtemp.setWaitForConversion(false); //non-blocking
    dtemp.requestTemperatures();

    encoders.setDebounce(16);
    // addEncoder(Encoder number, 4 steps per click, pin, number of modes, returned code, returned switch code )
    encoders.addEncoder(1, 4, A3, 1, 100,   1); // set RG
    encoders.addEncoder(2, 4, A4, 1, 200,   2); // set Amber
    encoders.addEncoder(3, 4, A5, 3, 300,   3); // change mode

  
    timerId = timer.setInterval(timerInterval, writeStimulus);

    //initial values for normal trichromat
    rgRatio = 0.402;  // R/G ratio
    valueAmber = valueAmberLum20;
    
    setupPWM16();
    analogWriteHR(pinBlue, 0);
  
    calcRG();
    displayVars();
}
//############################
void displayVars() {
    lcd.setCursor(14, 0);
    if (mode == 0)      lcd.print("Main  ");
    else if (mode == 1) lcd.print("SetLum ");
    else if (mode == 2) lcd.print("Static");

    lcd.setCursor(0, 0);
    lcd.print("RG=" + numToStrLen((int)(1000.0 * rgRatio), 4) + "(." + numToStrLen((int)1000.0*rgStep, 3) + ")");
    lcd.setCursor(0, 1);
    lcd.print("G=" + numToStrLen(valueGreen, 5) +  ";R=" + numToStrLen(valueRed, 5));
    lcd.setCursor(0, 2);
    lcd.print("A=" + numToStrLen(valueAmber, 5) + "(" + numToStrLen(stepAmber, 3) + ")");
    lcd.setCursor(13, 2);
    lcd.print("Lum=" + numToStrLen((int)1.0*valueLum, 2));

    lcd.setCursor(0, 3);
    lcd.print(String("Int=") + timerInterval + String("  "));
}
//####################################################################
void displayTemp() {
    lcd.setCursor(9, 3);
    lcd.print("T=" + String(fTemp) + (char)223); // 223 - Celsius
}
//############################
void calcRG() { // given rgRatio, Lum, calculate valueRed, valueGreen
    valueRed = (unsigned long)((float)rgRatio*valueLum/20.0*valueRedLum20);
    if (valueRed > maxValue) valueRed = maxValue;
    valueGreen = (unsigned long)((float)(1.0 - rgRatio)*valueLum/20.0*valueGreenLum20);
    if (valueGreen > maxValue) valueGreen = maxValue;
}
//############################
void writeStimulus() {
    if (autoChangePhase)
        phase = 1 - phase;
    if (phase == 0) {
      if (flagShowFlipFlop) {
          lcd.setCursor(18, 1);
          lcd.print("RG");
          lcd.setCursor(18, 3);
          lcd.print("  ");
          lcd.display();
      }
      analogWriteHR(pinRed,   valueRed);
      analogWriteHR(pinGreen, valueGreen);
      analogWriteHR(pinAmber, 0);
    }
    if (phase == 1) {
        if (flagShowFlipFlop) {
            lcd.setCursor(18, 1);
            lcd.print("  ");
            lcd.setCursor(18, 3);
            lcd.print("Am");
            lcd.display();
            if (mode != 2) myDFPlayer.play(1);  //Play the first mp3
        }
        analogWriteHR(pinRed,   0);
        analogWriteHR(pinGreen, 0);
        analogWriteHR(pinAmber, valueAmber);
    }
}
//############################
void changeTimerInterval() {
    switch (timerInterval) {
        case 1500: {
            timerInterval = 500;
            flagShowFlipFlop = true;
            break;
        }
        case  500: {
            timerInterval = 100;
            flagShowFlipFlop = false;
            lcd.setCursor(18, 1); lcd.print("  ");
            lcd.setCursor(18, 3); lcd.print("  ");
            lcd.display();
            break;
        }
        case  100: {
            timerInterval = 1500;
            flagShowFlipFlop = true;
        }
    }
    timer.deleteTimer(timerId);
    timerId = timer.setInterval(timerInterval, writeStimulus);
}
//############################
void loop() {
    timer.run(); // this function must be called inside loop() -- comment from SimpleTimer.h

    if (dtemp.isConversionComplete()) {
        fTemp=dtemp.getTempCByIndex(0);
        displayTemp();
        dtemp.requestTemperatures();
    }

    int code = encoders.readAll();
    if (code != 0) {
        if (code == 1) {                             // encoder 1 click -- set rgStep
            if (rgStep == rgStepSmall)       rgStep = rgStepMedium;
            else if (rgStep == rgStepMedium) rgStep = rgStepBig;
            else if (rgStep == rgStepBig)    rgStep = rgStepSmall;
        }
        if (code == 100) {                           // encoder 1 rotate CCW -- decrease RG
            if (rgRatio > rgStep) rgRatio -= rgStep;
            else                  rgRatio = 0.0;
            calcRG();
            if (mode == 2) {
                writeStimulus();
            }
        }
        if (code == 101 && rgRatio < 1.0) {          // encoder 1 rotate CW  -- increase RG
            rgRatio += rgStep;
            if (rgRatio > 1.0) rgRatio = 1.0;
            calcRG();
            if (mode == 2) {
                writeStimulus();
            }
        }
        if (code == 2) {                            // encoder 2 click - set Amber value step
            if (stepAmber == stepAmberSmall)       stepAmber = stepAmberMedium;
            else if (stepAmber == stepAmberMedium) stepAmber = stepAmberBig;
            else if (stepAmber == stepAmberBig)    stepAmber = stepAmberSmall;
        }
        if (code == 200) {                          // encoder 2 rotate CCW - decrease Amber
            if (valueAmber > stepAmber) valueAmber -= stepAmber;
            else                        valueAmber = 0;
            calcRG();
            if (mode == 2) {
                writeStimulus();
            }
        }
        if (code == 201 && valueAmber < maxValue) { // encoder 2 rotate CW - increase Amber
            valueAmber += stepAmber;
            if (valueAmber > maxValue) valueAmber = maxValue;
            calcRG();
            if (mode == 2) {
                writeStimulus();
            }
        }
    
        if (code == 3) {                           // encoder 3 click - change mode
            if (mode == 0) {
                mode = 1;
            }
            else if (mode == 1) {
                mode = 2;
                timer.disable(timerId); //Static
                autoChangePhase = false;
            }  
            else if (mode == 2) {
                mode = 0;
                timer.enable(timerId);
                autoChangePhase = true;
            }
        }
        if (mode == 0) { //main mode
            if (code == 300 || code == 301) changeTimerInterval();
        }
        if (mode == 1) { // set luminance mode
            if (code == 300 && valueLum > 0.0) { // encoder 3 rotate CCW - decrease Lum
                valueLum -= stepLum;
                if (valueLum < 0) valueLum = 0.0;
            }
            if (code == 301 && valueLum < maxLumGreen) {  // encoder 3 rotate CW - increase Lum
                valueLum += stepLum;
                if (valueLum > maxLumGreen) valueLum = (int)maxLumGreen;
            }
        }
        if (mode == 2) { // static mode
            if (code == 300 || code == 301) { // encoder 3 rotate CW/CCW - manual change phase
                phase = 1 - phase;
                writeStimulus();
            }
        }
        displayVars();
    }
}
