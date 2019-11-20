// AS ControlBox
// ######################################################
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h> // include i/o class header

#include <CommonBusEncoders.h>
#include <SimpleTimer.h>

// declare the lcd object
hd44780_I2Cexp lcd; // auto locate and autoconfig interface pins

// tell the hd44780 sketch the lcd object has been declared
#define HD44780_LCDOBJECT

#define LCD_COLS 20
#define LCD_ROWS 4

CommonBusEncoders encoders(A0, A1, A2, 3);

// There must be one global SimpleTimer object.
SimpleTimer timer;

long enc1;
long enc2;
long enc3;
int stepEnc1;
int stepEnc2;
int stepEnc3;
int stepEncSmall = 1;
int stepEncBig = 4;
int stepEncExtraBig = 16;
uint16_t encMaxValue;

int resBits = 10; //or 16
const uint16_t maxValue10 =  1023; //10-bit resolution
const uint16_t maxValue16 = 65535; //16-bit resolution
uint16_t maxValue;

int phase = 0;


//Pins
int pinGreen = 10;
int pinRed   =  9;
int pinAmber =  11;
int pinBlue  =  5;

uint16_t brRed = 0;
uint16_t brGreen = 0;
uint16_t brAmber = 0;

int RGRatio = 0;

bool flagShowFlipFlop = true;

uint16_t brRedMax;

uint16_t rgBr = 100;
bool rgBrSetup = false;

int timerId;
int timerInterval = 1500;
bool intervalChange = false;

/* Configure digital pins 9, 10, 11 as 10-bit PWM outputs. */
void setupPWM10() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) /* non-inverting PWM */
        | _BV(WGM11) | _BV(WGM10);                 /* mode 3: Phase correct, 10-bit */

  TCCR1B = _BV(CS11); //prescaler=8 Freq = 976Hz

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1) /* non-inverting PWM */
        | _BV(WGM31) | _BV(WGM30);                 /* mode 3: Phase correct, 10-bit */
  TCCR3B = _BV(CS31); //prescaler=8 Freq = 976Hz
}
/* Configure digital pins 9, 10, 11 and 5 as 16-bit PWM outputs. */
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
/* HR-bit version of analogWrite(). Works only on pins 9, 10, 11 */
void analogWriteHR(uint8_t pin, uint16_t val) {
  switch (pin) {
    case  9: OCR1A = val; break; //Red
    case 10: OCR1B = val; break; //Green
    case 11: OCR1C = val; break; //Amber
    case  5: OCR3A = val; break; //Blue
  }
}
//######################################
String toStr4(int i) {
  String result = "";
  if (i < 10) result += "0";
  if (i < 100) result += "0";
  if (i < 1000) result += "0";
  result += i;
  return result;
}
//######################################
String toStrN(int n, uint16_t i) {
  String result = "";
  for (int pos=1; pos < n; pos++) {
    if (i < pow(10,pos)) result += "0";
  }
  result += i;
  return result;
}
//######################################
void setup() {
  Serial.begin(115200);

  lcd.begin(LCD_COLS, LCD_ROWS);

  encoders.setDebounce(16);
  encoders.addEncoder(1, 4, A3, 1, 100,   1);
  encoders.addEncoder(2, 4, A4, 1, 200,   2);
  encoders.addEncoder(3, 4, A5, 1, 300,   3);

  timerId = timer.setInterval(timerInterval, setBrightness);

  resBits = 10;

  if (resBits == 10) {
    maxValue = maxValue10;
    setupPWM10();
    stepEncSmall = 1;
    stepEncBig = 4;
  }
  if (resBits == 16) {
    maxValue = maxValue16;
    setupPWM16();
    stepEncSmall = 16;
    stepEncBig = 256;
  }

  encMaxValue = maxValue;
  enc1 = 0.5*maxValue; // R/G
  enc2 = 0.7*0.5*maxValue;  //Amber
  enc3 = maxValue; // rgBr
  stepEnc1 = stepEncSmall;
  stepEnc2 = stepEncSmall;
  stepEnc3 = 16;

  brRedMax = 0.3*maxValue; //for SB1 from spectra measurements
                           //to equate luminance of pure G and pure R

  calcBr();
  updateDisplay();
}
//############################
void calcBr() {
  float ratio = 1.0 * (float)enc1 / (float)encMaxValue;
  if (ratio > 1.0) ratio = 1.0;
  RGRatio = (int)(1000.0 * ratio);
  brRed = (int)((float)brRedMax * ratio*(float)rgBr/100.0);
  brGreen = (int)((float)maxValue * (1.0 - ratio)*(float)rgBr/100.0);
  brAmber = enc2;
  if (rgBrSetup) {
    rgBr = enc3 / 1023.0 * 100.0;
  }
  if (intervalChange) {
    switch (timerInterval) {
      case 1500: {
          timerInterval = 500;
          flagShowFlipFlop = true;
          break;
        }
      case  500: {
          timerInterval = 100;
          flagShowFlipFlop = false;
          break;
        }
      case  100: {
          timerInterval = 1500;
          flagShowFlipFlop = true;
        }
    }
    timer.deleteTimer(timerId);
    timerId = timer.setInterval(timerInterval, setBrightness);
    intervalChange = false;
  }
}
//############################
void updateDisplay() {
  int N = 4;
  if(resBits == 10) N=4;
  if(resBits == 16) N=5;
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(String("RG=") + toStrN(N, RGRatio) + String("(") + stepEnc1 + String(")"));

  lcd.setCursor(0, 1);
  lcd.print(String("G=") + toStrN(N, brGreen) +  String(";R=") + toStrN(N, brRed));


  lcd.setCursor(0, 2);
  lcd.print(String("A=") + toStrN(N, brAmber) + String("(") + stepEnc2 + String(")"));

  lcd.setCursor(0, 3);
//  if (rgBrSetup) lcd.print(String("->"));
//  lcd.print(String("rgBr=") + rgBr);
  lcd.print(String("Interval=") + timerInterval);
  
}
//############################
void setBrightness() {
  calcBr();
  if (phase == 1) {
    if (flagShowFlipFlop) {
      lcd.setCursor(18, 1);
      lcd.print("RG");
      lcd.setCursor(18, 3);
      lcd.print("  ");
      lcd.display();
    }
    analogWriteHR(pinRed,   brRed);
    analogWriteHR(pinGreen, brGreen);
    analogWriteHR(pinAmber, 0);

  }
  if (phase == 0) {
    if (flagShowFlipFlop) {
      lcd.setCursor(18, 1);
      lcd.print("  ");
      lcd.setCursor(18, 3);
      lcd.print("AB");
      lcd.display();
    }
    analogWriteHR(pinRed,   0);
    analogWriteHR(pinGreen, 0);
    analogWriteHR(pinAmber, brAmber);
  }
  phase = 1 - phase;
}
//############################
void loop() {
  timer.run();
  int code = encoders.readAll();
  if (code != 0) {
    //    Serial.println(code);
    if (code == 1) {
      if      (stepEnc1 == stepEncSmall)    stepEnc1 = stepEncBig;
      else if (stepEnc1 == stepEncBig)      stepEnc1 = stepEncExtraBig;
      else if (stepEnc1 == stepEncExtraBig) stepEnc1 = stepEncSmall;
    }
    if (code == 100) {
      if (enc1 > stepEnc1) enc1 -= stepEnc1; else enc1 = 0;
    }
    if (code == 101 && enc1 < encMaxValue) {
      enc1 += stepEnc1;
      if (enc1 > encMaxValue) enc1 = encMaxValue;
    }
    if (code == 2) {
      if (stepEnc2 == stepEncSmall) stepEnc2 = stepEncBig;
      else stepEnc2 = stepEncSmall;
    }
    if (code == 200) {
      if (enc2 > stepEnc2) enc2 -= stepEnc2; else enc2 = 0;
    }
    if (code == 201 && enc2 < encMaxValue) {
      enc2 += stepEnc2;
      if (enc2 > encMaxValue) enc2 = encMaxValue;
    }
    if (code == 3) {
      //rgBrSetup = !rgBrSetup;
      intervalChange = true;
    }
    if (code == 300 && enc3 > 0) {
      enc3 -= stepEnc3;
      if (enc3 < 0) enc3 = 0;
    }
    if (code == 301 && enc3 < encMaxValue) {
      enc3 += stepEnc3;
      if (enc3 > encMaxValue) enc3 = encMaxValue;
    }
    calcBr();
    updateDisplay();
  }
}
