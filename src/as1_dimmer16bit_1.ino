#define VERSION "as1_dimmer16bit_1"

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
//encoders(BusA, BusB,BusSwitches, Number of encoders)
CommonBusEncoders encoders(A0, A1, A2, 3);

//-------------------------------------------
const unsigned long maxValue =  65535; //16-bit resolution

unsigned int stepTiny     =    1;
unsigned int stepSmall    =    4;
unsigned int stepBig      =   16;
unsigned int stepExtraBig =  256;
unsigned int stepHuge     = 1024;

struct colorStruct {
  int pin;
  char name[6];
  unsigned long value;
  unsigned int step;
};

#define NUM_COLORS 4
struct colorStruct colors[NUM_COLORS] = {
{ 9, "Red  ", 0, stepExtraBig},
{10, "Green", 0, stepExtraBig},
{ 5, "Blue ", 0, stepExtraBig},
{11, "Amber", 0, stepExtraBig}
};

int colorIndex = 0;
struct colorStruct color = colors[colorIndex];

float fTemp;

// Configure digital pins 9, 10, 11, 5 as 10-bit PWM outputs. Freq = 976Hz -- Arduino Leonardo --
void setupPWM10() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) /* non-inverting PWM */
        | _BV(WGM11) | _BV(WGM10);                 /* mode 3: Phase correct, 10-bit */

  TCCR1B = _BV(CS11); //prescaler=8

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1)                             /* non-inverting PWM */
        | _BV(WGM31) | _BV(WGM30);                 /* mode 3: Phase correct, 10-bit */
  TCCR3B = _BV(CS31); //prescaler=8
}
// Configure digital pins 9, 10, 11, and 5 as 16-bit PWM outputs. Freq = 244 Hz -- Arduino Leonardo --
void setupPWM16() {
  DDRB  |= _BV(PB5) | _BV(PB6) | _BV(PB7); //pins 9,10,11
  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1C1) /* non-inverting PWM */
         | _BV(WGM11);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); //prescaler=1
  ICR1 = 0xFFFF;

  DDRC  |= _BV(PC6); //pin5
  TCCR3A = _BV(COM3A1) /* non-inverting PWM */
        | _BV(WGM11);                 /* mode 14: fast PWM, TOP=ICR1 */
  TCCR3B = _BV(WGM33) | _BV(WGM32) | _BV(CS30); //prescaler=1
  ICR3 = 0xFFFF;
}
/* HR-bit version of analogWrite(). Works only on pins 9, 10, 11, 5 */
void analogWriteHR(int pin, unsigned int val) {
  switch (pin) {
    case  9: OCR1A = val; break; //Red
    case 10: OCR1B = val; break; //Green
    case 11: OCR1C = val; break; //Amber
    case  5: OCR3A = val; break; //Blue
  }
}
//####################################################################
void setup() {
  //Serial.begin(115200);

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(String(VERSION));
  delay(5000);
  lcd.clear();

  dtemp.begin();
  dtemp.setWaitForConversion(false); //non-blocking
  dtemp.requestTemperatures();

  encoders.setDebounce(16);
  // addEncoder(Encoder number, steps per click=4, pin, number of modes=1, returned code, returned switch code )
  encoders.addEncoder(1, 4, A3, 1, 100,   1);
  encoders.addEncoder(2, 4, A4, 1, 200,   2);
  encoders.addEncoder(3, 4, A5, 1, 300,   3);

  setupPWM16();
  for(int i=0; i<NUM_COLORS; i++) {
    analogWriteHR(colors[i].pin, 0);
  }
  displayRGBA();
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
void displayRGBA() {
  analogWriteHR(color.pin, color.value);
//  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("R=" + numToStrLen(colors[0].value, 5) + "(" + numToStrLen(colors[0].step, 4) + ")");
  lcd.setCursor(0, 1);
  lcd.print("G=" + numToStrLen(colors[1].value, 5) + "(" + numToStrLen(colors[1].step, 4) + ")");
  lcd.setCursor(0, 2);
  lcd.print("B=" + numToStrLen(colors[2].value, 5) + "(" + numToStrLen(colors[2].step, 4) + ")");
  lcd.setCursor(0, 3);
  lcd.print("A=" + numToStrLen(colors[3].value, 5) + "(" + numToStrLen(colors[3].step, 4) + ")");

  lcd.setCursor(14, 0);
  lcd.print("Color=");
  lcd.setCursor(15, 1);
  lcd.print(color.name);
}
//####################################################################
void displayTemp() {
  lcd.setCursor(13, 3);
//  lcd.print(String("T=") + String(fTemp) + (char)223);
  lcd.print(String("T=") + String(fTemp));
}
//####################################################################
unsigned int getNextStep(unsigned int step) { 
  if (step == stepTiny)
     step = stepSmall;
  else if (step == stepSmall)
     step = stepBig;
  else if (step == stepBig)
     step = stepExtraBig;
  else if (step == stepExtraBig)
     step = stepHuge;
  else if (step == stepHuge)
     step = stepTiny;
  return step;
}
//####################################################################
int getNextColorIndex(int direction, int colorIndex) {
  if (direction == +1) {
    if (colorIndex < NUM_COLORS-1) {
      colorIndex++;
    } else {
      colorIndex = 0;
    }
  } else { // direction == -1
    if (colorIndex == 0) {
      colorIndex = NUM_COLORS-1;
    } else {
      colorIndex--;
    }
  }
  return colorIndex;  
}
//####################################################################
void loop() {
  if (dtemp.isConversionComplete()) {
    fTemp=dtemp.getTempCByIndex(0);
    dtemp.requestTemperatures();
    displayTemp();
  }
  int code = encoders.readAll();
  if (code != 0) {
    color = colors[colorIndex];
    if (code == 1) color.step = getNextStep(color.step);
    if (code == 100) {
      if (color.value > color.step) {
        color.value -= color.step;
      } else {
        color.value = 0;
      }
    }
    if (code == 101 && color.value < maxValue) {
      color.value += color.step;
      if (color.value > maxValue) color.value = maxValue;
    }
    colors[colorIndex] = color;
    
    if (code == 200) {
      colorIndex = getNextColorIndex(-1, colorIndex);
      color = colors[colorIndex];
    }
    if (code == 201) {
      colorIndex = getNextColorIndex(+1, colorIndex);
      color = colors[colorIndex];
    }
    displayRGBA();
  }
}
