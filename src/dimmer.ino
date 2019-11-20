#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h> // include i/o class header
#include <CommonBusEncoders.h>

// declare the lcd object
hd44780_I2Cexp lcd; // auto locate and autoconfig interface pins

// tell the hd44780 sketch the lcd object has been declared
#define HD44780_LCDOBJECT

#define LCD_COLS 20
#define LCD_ROWS 4

CommonBusEncoders encoders(A0, A1, A2, 3);

uint16_t enc1;
uint16_t enc2;
uint16_t enc3;
int stepEnc1;
int stepEnc2;
int stepEnc3;
int stepEncSmall = 1;
int stepEncBig = 4;
int stepEncExtraBig = 16;
uint16_t encMaxValue;

int resBits = 10;
const uint16_t maxValue10 =  1023; //10-bit resolution
const uint16_t maxValue16 = 65535; //16-bit resolution
uint16_t maxValue;

//Pins
int pinRed   =   9;
int pinGreen =  10;
int pinAmber =  11;
int pinBlue  =  5;

uint16_t brRed   = 0;
uint16_t brGreen = 0;
uint16_t brAmber  = 0;
uint16_t brBlue  = 0;

/* Configure digital pins 9, 10, 11, 5 as 10-bit PWM outputs. */
// for Leonardo
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

/* HR-bit version of analogWrite(). Works only on pins 9, 10, 11 */
void analogWriteHR(uint8_t pin, uint16_t val) {
  switch (pin) {
    case  9: OCR1A = val; break; //Red
    case 10: OCR1B = val; break; //Green
    case 11: OCR1C = val; break; //Amber
    case  5: OCR3A = val; break; //Blue
  }
}
//####################################################################
void setup() {
  // put your setup code here, to run once:
//  Serial.begin(115200);

  lcd.begin(LCD_COLS, LCD_ROWS);
  encoders.setDebounce(16);
  encoders.addEncoder(1, 4, A3, 1, 100,   1);
  encoders.addEncoder(2, 4, A4, 1, 200,   2);
  encoders.addEncoder(3, 4, A5, 1, 300,   3);

  resBits = 10;

  if (resBits == 10) {
    maxValue = maxValue10;
    setupPWM10();
    stepEncSmall = 1;
    stepEncBig = 4;
  }
  if (resBits == 16) {
    maxValue = maxValue16;
//    setupPWM16();
    stepEncSmall = 16;
    stepEncBig = 256;
  }

  encMaxValue = maxValue;
  enc1 = 0; // Red
  enc2 = 127; //Green
  enc3 = 0; // Blue
  brRed   = enc1;
  brGreen = enc2;
  brAmber  = enc3;
  //brBlue  = enc3;

  stepEnc1 = stepEncBig;
  stepEnc2 = stepEncBig;
  stepEnc3 = stepEncBig;

  updateDisplay();
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
//####################################################################
void setBrightness() {
  brRed   = enc1;
  brGreen = enc2;
  brAmber  = enc3;
//  brBlue  = enc3;
  analogWriteHR(pinRed,   brRed);
  analogWriteHR(pinGreen, brGreen);
  analogWriteHR(pinAmber,  brAmber);
//  analogWriteHR(pinBlue,  brBlue);
}
//####################################################################
void updateDisplay() {
  setBrightness();
  int N = 4;
  if(resBits == 10) N=4;
  if(resBits == 16) N=5;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(String("R=") + toStrN(N, brRed) + String("(") + stepEnc1 + String(")"));
  lcd.setCursor(0, 1);
  lcd.print(String("G=") + toStrN(N, brGreen) + String("(") + stepEnc2 + String(")"));
  lcd.setCursor(0, 2);
  lcd.print(String("A=") + toStrN(N, brAmber) + String("(") + stepEnc3 + String(")"));
//  lcd.print(String("B=") + toStrN(N, brBlue) + String("(") + stepEnc3 + String(")"));
}
//####################################################################
void loop() {
  int code = encoders.readAll();
  if (code != 0) {
    //Serial.println(code);
    if (code == 1) {
      if (stepEnc1 == stepEncSmall) stepEnc1 = stepEncBig;
      else if
         (stepEnc1 == stepEncBig) stepEnc1 = stepEncExtraBig;
      else if
         (stepEnc1 == stepEncExtraBig) stepEnc1 = stepEncSmall;
    }
    if (code == 100) {
      if (enc1 > stepEnc1) enc1 -= stepEnc1; else enc1 = 0;
    }
    if (code == 101 && enc1 < encMaxValue) { enc1 += stepEnc1; if (enc1 > encMaxValue) enc1 = encMaxValue;}
    
    if (code == 2) {
      if (stepEnc2 == stepEncSmall) stepEnc2 = stepEncBig;
      else if
         (stepEnc2 == stepEncBig) stepEnc2 = stepEncExtraBig;
      else if
         (stepEnc2 == stepEncExtraBig) stepEnc2 = stepEncSmall;
    }

    if (code == 200) {
       if(enc2 > stepEnc2) enc2 -= stepEnc2; else enc2 = 0;
    }
    if (code == 201 && enc2 < encMaxValue) { enc2 += stepEnc2; if (enc2 > encMaxValue) enc2 = encMaxValue;}
    
    if (code == 3) {
      if (stepEnc3 == stepEncSmall) stepEnc3 = stepEncBig;
      else if
         (stepEnc3 == stepEncBig) stepEnc3 = stepEncExtraBig;
      else if
         (stepEnc3 == stepEncExtraBig) stepEnc3 = stepEncSmall;
    }
    if (code == 300) {
      if (enc3 > stepEnc3) enc3 -= stepEnc3; else enc3 = 0;
    }
    if (code == 301 && enc3 < encMaxValue) { enc3 += stepEnc3; if (enc3 > encMaxValue) enc3 = encMaxValue;}
    updateDisplay();
  }
}
