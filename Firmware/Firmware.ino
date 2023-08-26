/**************************************************************************
#define PIN_PA0 0 RX_CP2102
#define PIN_PA1 1 TX_CP2102
#define PIN_PA2 2
#define PIN_PA3 3
#define PIN_PA4 4 MOSI_Display
#define PIN_PA5 5
#define PIN_PA6 6 SCK_Display
#define PIN_PA7 7
#define PIN_PB0 8
#define PIN_PB1 9
#define PIN_PB2 10
#define PIN_PB3 11
#define PIN_PB4 12
#define PIN_PB5 13
#define PIN_PC0 14
#define PIN_PC1 15
#define PIN_PC2 16
#define PIN_PC3 17
#define PIN_PC4 18
#define PIN_PC5 19
#define PIN_PC6 20
#define PIN_PC7 21
#define PIN_PD0 22
#define PIN_PD1 23
#define PIN_PD2 24
#define PIN_PD3 25
#define PIN_PD4 26
#define PIN_PD5 27 HSync
#define PIN_PD6 28 VSync
#define PIN_PD7 29 
#define PIN_PE0 30
#define PIN_PE1 31
#define PIN_PE2 32
#define PIN_PE3 33 TCS_Display
#define PIN_PF0 34
#define PIN_PF1 35 Odd/Even
#define PIN_PF2 36
#define PIN_PF3 37
#define PIN_PF4 38 DC_Display
#define PIN_PF5 39 RST_Display
#define PIN_PF6 40 CP2021


BITMAP 4Byte / 32 Bit - int32_t

00000000 00000000 00000000 00000000

SCART RGBS  = XXXXXXXX XXXXXXXX XXXX0000 0X100100
SCART YPbPr = XXXXXXXX XXXXXXXX XXXX0000 0X111100
SCART RGsB  = XXXXXXXX XXXXXXXX XXXX0000 0X110100

COMP YPbPr  = XXXXXXXX XXXXXXXX XXXX1000 0X111100
COMP RGsB   = XXXXXXXX XXXXXXXX XXXX1000 0X110100

VGA RGBHV   = XXXXXXXX XXXXXXXX XX010010 0X101000

**************************************************************************/

#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>
#include <avr/io.h>
#include <math.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "Button.h"
#include "Timer.h"

#define VIDEO_AMP_BYPASS 24
#define VIDEO_AMP_DISABLE 23

#define CONVERTER_SELECT 21
#define CONVERTER_DETECT 20
#define CONVERTER_HD 19
#define CONVERTER_POWERSAVE 18

#define VIDEO_MUX_1_ENABLE 14
#define VIDEO_MUX_1_SELECT 15
#define VIDEO_MUX_2_ENABLE 16
#define VIDEO_MUX_2_SELECT 17

#define CSYNC_MUX_1 10
#define CSYNC_MUX_2 11
#define HSYNC_MUX_IN 7
#define HSYNC_MUX_OUT 37

#define AUDIO_MUX_1 25
#define AUDIO_MUX_2 26

#define FG_COLOR 0xFFFF
#define BG_COLOR 0x0000
#define SL_COLOR 0xC409
#define HS_TEXT "HSync:"
#define VS_TEXT "VSync:"
#define VD_TEXT "Video: "
#define AD_TEXT "Audio: "
#define HZ_TEXT "Hz"
#define KHZ_TEXT "kHz"
#define PROGRESSIVE_TEXT "p"
#define INTERLACED_TEXT "i"
#define NO_SYNC_TEXT_1 "No"
#define NO_SYNC_TEXT_2 "Sync"

bool settingsMenu = false;

float tempHorizontalFreq = 0.0f;
float tempHorizontalPos = 0.0f;
float tempHorizontalNeg = 0.0f;

float tempVerticalFreq = 0.0f;
float tempVerticalPos = 0.0f;
float tempVerticalNeg = 0.0f;

float tempInterlacedFreq = 0.0f;
float tempInterlacedPos = 0.0f;
float tempInterlacedNeg = 0.0f;

Button But1(0);
Button But2(1);
Button But3(2);
Button But4(3);

Adafruit_ST7789 TFT = Adafruit_ST7789(33, 38, 39);

SoftwareTimer timingHSyncTimer;
SoftwareTimer timingVSyncTimer;
SoftwareTimer timingOddEvenTimer;

HardwareTimer<0> timerTCB0;
HardwareTimer<1> timerTCB1;
HardwareTimer<3> timerTCB3;


int8_t avSetting = 0b00110100;  // first nibble audio / last nibble video and format

bool videoPinsSet = false;
bool audioPinsSet = false;

int32_t selectedPreset = 0;



struct Timings {

  Timings() {}

public:
  float horizontal = 0.0f;
  float vertical = 0.0f;
  float interlaced = 0.0f;
  int32_t lineCount = 0;

  bool locked = false;

private:

  String lastPrinted = "";
  int32_t lastX = 0;
  int32_t lastY = 0;
  int32_t lastW = 0;
  int32_t lastH = 0;

public:

  setLinecount(int32_t count){
    if(lineCount == count)
      return;

    lineCount = count;

    if(locked)
      printLinecount();

  }

  setLock(bool state){

    if(locked == state)
      return;

    locked = state;

    if(!locked){
      lineCount = 0;
      printLinecount();
    }
    else
    {
      printLinecount();
    }
    
  }

private:

  void centerText(int32_t width, int32_t height, int32_t& x, int32_t& y, int32_t fontSize, String str){
    
    int32_t textWidth = strlen(str.c_str()) * 6 * fontSize;
    int32_t textHeight = 8 * fontSize;

    x = (width / 2 - textWidth / 2) + 1;
    y = height / 2 - textHeight / 2;

  }

  void printLinecount(){

    TFT.setTextColor(FG_COLOR);
    TFT.setTextSize(4);

    if(lineCount == 0 || !locked){
      if(lastPrinted == NO_SYNC_TEXT_1)
        return;
    }
    else if(String(lineCount) == lastPrinted)
      return;

    TFT.fillRect(lastX, lastY, lastW, lastH, BG_COLOR);

    if(lineCount == 0 || !locked){

      lastPrinted = NO_SYNC_TEXT_1;

      TFT.setCursor(58, 26);
      TFT.print(NO_SYNC_TEXT_1);
      TFT.setCursor(34, 62);
      TFT.print(NO_SYNC_TEXT_2);

      lastX = 34;
      lastY = 26;
      lastW = 96;
      lastH = 68;
      return;
    }

    String lcStr = "";

    if(interlaced != 0.0f)
      lcStr = String(lineCount) + INTERLACED_TEXT;
    else
      lcStr = String(lineCount) + PROGRESSIVE_TEXT;

    
    int32_t x;
    int32_t y;
    centerText(163, 120, x, y, 4, lcStr);

    lastPrinted = lcStr;
    lastX = x;
    lastY = y;
    lastW = strlen(lcStr.c_str()) * 6 * 4;
    lastH = 8 * 4;

    TFT.setCursor(x, y);
    TFT.print(lcStr);
  }
};

struct TimingSample {

  TimingSample() {}

  float horizontal = 0.0f;
  float vertical = 0.0f;
  float interlaced = 0.0f;
};

TimingSample timingSamples[10];
uint32_t tSampleIndex = 0;

Timings timings;

//{HW Timer
  void initEventSystem(void) {
    EVSYS.CHANNEL2 = EVSYS_GENERATOR_PORT1_PIN5_gc;  // connect Pin 'PD5' to channel 2
    EVSYS.USERTCB0 = EVSYS_CHANNEL_CHANNEL2_gc;      // connect 'generator 'PD5' with user 'TCB0' via channel 2

    EVSYS.CHANNEL3 = EVSYS_GENERATOR_PORT1_PIN6_gc;
    EVSYS.USERTCB1 = EVSYS_CHANNEL_CHANNEL3_gc;

    EVSYS.CHANNEL4 = EVSYS_GENERATOR_PORT1_PIN1_gc;
    EVSYS.USERTCB3 = EVSYS_CHANNEL_CHANNEL4_gc;
  }


  void initTCB0(void) {
    timerTCB0.Reset();
    timerTCB0.SetCLKDIV1();
    timerTCB0.SetModeFRQPW();
    timerTCB0.EnableEventInput();
    timerTCB0.EnableCAPTInterrupt();
    timerTCB0.Enable();
  }

  void initTCB1(void) {
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc | TCA_SINGLE_ENABLE_bm;
    timerTCB1.Reset();
    timerTCB1.SetCLKTCA();
    timerTCB1.SetModeFRQPW();
    timerTCB1.EnableEventInput();
    timerTCB1.EnableCAPTInterrupt();
    timerTCB1.Enable();
  }

  void initTCB3(void) {
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc | TCA_SINGLE_ENABLE_bm;
    timerTCB3.Reset();
    timerTCB3.SetCLKTCA();
    timerTCB3.SetModeFRQPW();
    timerTCB3.EnableEventInput();
    timerTCB3.EnableCAPTInterrupt();
    timerTCB3.Enable();
  }

  ISR(TCB0_INT_vect) {
    timerTCB0.cnt = TCB0.CNT;  // read out CNT first, then CCMP
    timerTCB0.ccmp = TCB0.CCMP;
    timerTCB0.finish = true;
    timerTCB0.DeleteFlagCAPT();  // clear the interrupt flag
  }

  ISR(TCB1_INT_vect) {
    timerTCB1.cnt = TCB1.CNT;  // read out CNT first, then CCMP
    timerTCB1.ccmp = TCB1.CCMP;
    timerTCB1.finish = true;
    timerTCB1.DeleteFlagCAPT();  // clear the interrupt flag
  }

  ISR(TCB3_INT_vect) {
    timerTCB3.cnt = TCB3.CNT;  // read out CNT first, then CCMP
    timerTCB3.ccmp = TCB3.CCMP;
    timerTCB3.finish = true;
    timerTCB3.DeleteFlagCAPT();  // clear the interrupt flag
  }
//}


void printBackground() {
  TFT.fillRect(0, 0, 320, 170, BG_COLOR);
  TFT.fillRect(164, 0, 3, 121, FG_COLOR);
  TFT.fillRect(0, 120, 320, 3, FG_COLOR);

  TFT.setTextColor(FG_COLOR);
  TFT.setTextSize(3);

  TFT.setCursor(173, 6);
  TFT.print(HS_TEXT);
  TFT.setCursor(173, 63);
  TFT.print(VS_TEXT);
  TFT.setCursor(263, 36);
  TFT.print(KHZ_TEXT);
  TFT.setCursor(282, 93);
  TFT.print(HZ_TEXT);

  //placeholder
  TFT.setCursor(173, 36);
  TFT.print("00.00");
  TFT.setCursor(191, 93);
  TFT.print("00.00");

  TFT.setTextSize(2);

  TFT.setCursor(6, 129);
  TFT.print(VD_TEXT);
  TFT.setCursor(6, 149);
  TFT.print(AD_TEXT);

  digitalWrite(CONVERTER_DETECT, HIGH);
  digitalWrite(CONVERTER_POWERSAVE, LOW);
  digitalWrite(VIDEO_AMP_BYPASS, LOW);
  digitalWrite(VIDEO_AMP_DISABLE, LOW);
}

void getTimings() {
  static uint32_t lastMillis{ 0 };

  if (timerTCB0.finish) {
    uint16_t period{ 0 };
    uint16_t pulse{ 0 };
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      period = timerTCB0.cnt;
      pulse = timerTCB0.ccmp;
    }
    tempHorizontalFreq = 1 / (62.5 * 1.0 * (float)period / 1000000000UL) / 1000;
    tempHorizontalPos = 62.5 * 1.0 * (float)pulse / 1000UL;
    tempHorizontalNeg = (62.5 * 1.0 * (float)period / 1000UL) - tempHorizontalPos;
    timerTCB0.finish = false;

    timingHSyncTimer.Start();
  }

  if (timerTCB1.finish) {
    uint16_t period{ 0 };
    uint16_t pulse{ 0 };
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      period = timerTCB1.cnt;
      pulse = timerTCB1.ccmp;
    }
    tempVerticalFreq = 1 / (62.5 * 64.0 * (float)period / 1000000000UL);
    tempVerticalPos = 62.5 * 64.0 * (float)pulse / 1000000UL;
    tempVerticalNeg = (62.5 * 64.0 * (float)period / 1000000UL) - tempVerticalPos;
    timerTCB1.finish = false;

    timingVSyncTimer.Start();
  }

  if (timerTCB3.finish) {
    uint16_t period{ 0 };
    uint16_t pulse{ 0 };
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      period = timerTCB3.cnt;
      pulse = timerTCB3.ccmp;
    }
    tempInterlacedFreq = 1 / (62.5 * 64.0 * (float)period / 1000000000UL);
    tempInterlacedPos = 62.5 * 64.0 * (float)pulse / 1000000UL;
    tempInterlacedNeg = (62.5 * 64.0 * (float)period / 1000000UL) - tempInterlacedPos;
    timerTCB3.finish = false;

    timingOddEvenTimer.Start();
  }
}

void drawTimings() {

  int32_t dotIndex;

  TFT.setTextColor(FG_COLOR, BG_COLOR);
  TFT.setTextSize(3);

  if (!timings.locked || timings.horizontal == 0.0f || timings.vertical == 0.0f) {
    TFT.setCursor(173, 36);
    TFT.print("00.00");

    TFT.setCursor(191, 93);
    TFT.print("00.00");
    return;
  }

  String h;
  h = String(timings.horizontal);
  dotIndex = h.indexOf('.');
  h = h.substring(dotIndex - 2, 5);

  String v;
  v = String(timings.vertical);
  dotIndex = v.indexOf('.');
  v = v.substring(dotIndex - 2, 5);

  TFT.setCursor(173, 36);
  TFT.print(h);

  TFT.setCursor(191, 93);
  TFT.print(v);
}

void drawDiagTimings() {

  TFT.setTextColor(FG_COLOR, BG_COLOR);
  TFT.setTextSize(1);

  TFT.setCursor(0, 8);
  TFT.print(tempHorizontalFreq);
  TFT.setCursor(0, 16);
  TFT.print(tempHorizontalPos);
  TFT.setCursor(0, 24);
  TFT.print(tempHorizontalNeg);

  TFT.setCursor(0, 32);
  TFT.print(tempVerticalFreq);
  TFT.setCursor(0, 40);
  TFT.print(tempVerticalPos);
  TFT.setCursor(0, 48);
  TFT.print(tempVerticalNeg);

  TFT.setCursor(0, 56);
  TFT.print(tempInterlacedFreq);
  TFT.setCursor(0, 64);
  TFT.print(tempInterlacedPos);
  TFT.setCursor(0, 72);
  TFT.print(tempInterlacedNeg);
  
}

void setVideoPins() {

  //digitalWrite(VIDEO_AMP_DISABLE, HIGH);
  //delay(100);

  int8_t formatSetting = avSetting & 0b00000011;
  int8_t videoSetting = (avSetting & 0b00001100) >> 2;
  int8_t audioSetting = (avSetting & 0b00110000) >> 4;

  TFT.setTextColor(FG_COLOR, BG_COLOR);
  TFT.setTextSize(2);

  if (videoSetting == 1 && formatSetting == 0) {  // SCART - RGBS

    digitalWrite(VIDEO_MUX_1_SELECT, LOW);
    digitalWrite(VIDEO_MUX_2_SELECT, LOW);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, HIGH);
    digitalWrite(CSYNC_MUX_2, LOW);

    digitalWrite(CONVERTER_SELECT, LOW);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  }
  else if (videoSetting == 1 && formatSetting == 1) {  // SCART - RGsB

    digitalWrite(VIDEO_MUX_1_SELECT, LOW);
    digitalWrite(VIDEO_MUX_2_SELECT, LOW);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, HIGH);
    digitalWrite(CSYNC_MUX_2, HIGH);

    digitalWrite(CONVERTER_SELECT, LOW);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  }
  else if (videoSetting == 1 && formatSetting == 2) {  // SCART - YPbPr

    digitalWrite(VIDEO_MUX_1_SELECT, LOW);
    digitalWrite(VIDEO_MUX_2_SELECT, LOW);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, HIGH);
    digitalWrite(CSYNC_MUX_2, HIGH);

    digitalWrite(CONVERTER_SELECT, HIGH);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  } 
  else if (videoSetting == 2 && formatSetting == 0) {  // VGA - RGBS

    digitalWrite(VIDEO_MUX_1_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_2_SELECT, LOW);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, LOW);
    digitalWrite(CSYNC_MUX_2, HIGH);
    digitalWrite(HSYNC_MUX_IN, LOW);
    digitalWrite(HSYNC_MUX_OUT, HIGH);

    digitalWrite(CONVERTER_SELECT, LOW);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  } 
  else if (videoSetting == 2 && formatSetting == 1) {  // VGA - RGsB

    digitalWrite(VIDEO_MUX_1_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_2_SELECT, LOW);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, HIGH);
    digitalWrite(CSYNC_MUX_2, HIGH);

    digitalWrite(CONVERTER_SELECT, LOW);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  } 
  else if (videoSetting == 2 && formatSetting == 2) {  // VGA - YPbPr

    digitalWrite(VIDEO_AMP_BYPASS, HIGH);
    digitalWrite(VIDEO_AMP_DISABLE, LOW);

    digitalWrite(VIDEO_MUX_1_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_2_SELECT, LOW);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, HIGH);
    digitalWrite(CSYNC_MUX_2, HIGH);

    digitalWrite(CONVERTER_SELECT, HIGH);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  } 
  else if (videoSetting == 2 && formatSetting == 3) {  // VGA - RGBHV

    digitalWrite(VIDEO_AMP_BYPASS, LOW);
    digitalWrite(VIDEO_AMP_DISABLE, LOW);
    digitalWrite(VIDEO_MUX_1_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_2_SELECT, LOW);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, LOW);
    digitalWrite(CSYNC_MUX_2, HIGH);
    digitalWrite(HSYNC_MUX_IN, HIGH);
    digitalWrite(HSYNC_MUX_OUT, LOW);

    digitalWrite(CONVERTER_SELECT, LOW);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  } 
  else if (videoSetting == 3 && formatSetting == 1) {  // COMP - RGsB

    digitalWrite(VIDEO_MUX_1_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_2_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, HIGH);
    digitalWrite(CSYNC_MUX_2, HIGH);

    digitalWrite(CONVERTER_SELECT, LOW);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  }
  else if (videoSetting == 3 && formatSetting == 2) {  // COMP - YPbPr

    digitalWrite(VIDEO_AMP_BYPASS, HIGH);
    digitalWrite(VIDEO_AMP_DISABLE, LOW);

    digitalWrite(VIDEO_MUX_1_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_2_SELECT, HIGH);
    digitalWrite(VIDEO_MUX_1_ENABLE, LOW);
    digitalWrite(VIDEO_MUX_2_ENABLE, LOW);

    digitalWrite(CSYNC_MUX_1, HIGH);
    digitalWrite(CSYNC_MUX_2, HIGH);

    digitalWrite(CONVERTER_SELECT, HIGH);
    digitalWrite(CONVERTER_DETECT, HIGH);
    digitalWrite(CONVERTER_POWERSAVE, LOW);
  } 
  
  String videoInputName = "";

  if (videoSetting == 1) {
    videoInputName += "SCART";
  } 
  else if (videoSetting == 2) {
    videoInputName += "VGA";
  } 
  else if (videoSetting == 3) {
    videoInputName += "Component";
  }

  videoInputName += " - ";

  if (formatSetting == 0) {
    videoInputName += "RGBS";
  } 
  else if (formatSetting == 1) {
    videoInputName += "RGsB";
  } 
  else if (formatSetting == 2) {
    videoInputName += "YPbPr";
  } 
  else if (formatSetting == 3) {
    videoInputName += "RGBHV";
  }

  if(!settingsMenu) {
    TFT.fillRect(90, 129, 230, 16, BG_COLOR);
    TFT.setCursor(90, 129);
    TFT.print(videoInputName);
  }


  //delay(100);
  //digitalWrite(VIDEO_AMP_DISABLE, LOW);

  videoPinsSet = true;
}

void setAudioPins() {

  int8_t audioSetting = (avSetting & 0b00110000) >> 4;

  TFT.setTextColor(FG_COLOR, BG_COLOR);
  TFT.setTextSize(2);
  
  if (audioSetting == 1) {  // AUX
    digitalWrite(AUDIO_MUX_1, HIGH);
    digitalWrite(AUDIO_MUX_2, LOW);
  } 
  else if (audioSetting == 2) {  //RCA
    digitalWrite(AUDIO_MUX_1, LOW);
    digitalWrite(AUDIO_MUX_2, HIGH);
  } 
  else if (audioSetting == 3) {  //SCART
    digitalWrite(AUDIO_MUX_1, HIGH);
    digitalWrite(AUDIO_MUX_2, HIGH);
  }

  String audioInputName = "";

  if (audioSetting == 1) {
    audioInputName += "AUX";
  } else if (audioSetting == 2) {
    audioInputName += "RCA";
  } else if (audioSetting == 3) {
    audioInputName += "SCART";
  }

  if(!settingsMenu) {
    TFT.fillRect(90, 149, 230, 16, BG_COLOR);
    TFT.setCursor(90, 149);
    TFT.print(audioInputName);
  }

  audioPinsSet = true;
}

void setup() {
  Serial.begin(115200);
  TFT.init(170, 320, 0);
  TFT.setRotation(1);
  TFT.setTextWrap(false);
  printBackground();

  pinMode(3, INPUT);
  pinMode(2, INPUT);
  pinMode(1, INPUT);
  pinMode(0, INPUT);

  pinMode(HSYNC_MUX_IN, OUTPUT);
  pinMode(HSYNC_MUX_OUT, OUTPUT);
  pinMode(VIDEO_AMP_BYPASS, OUTPUT);
  pinMode(VIDEO_AMP_DISABLE, OUTPUT);
  pinMode(CSYNC_MUX_1, OUTPUT);
  pinMode(CSYNC_MUX_2, OUTPUT);
  pinMode(CONVERTER_SELECT, OUTPUT);
  pinMode(CONVERTER_DETECT, OUTPUT);
  pinMode(CONVERTER_HD, INPUT);
  pinMode(CONVERTER_POWERSAVE, OUTPUT);
  pinMode(VIDEO_MUX_1_ENABLE, OUTPUT);
  pinMode(VIDEO_MUX_2_ENABLE, OUTPUT);
  pinMode(VIDEO_MUX_1_SELECT, OUTPUT);
  pinMode(VIDEO_MUX_2_SELECT, OUTPUT);
  pinMode(AUDIO_MUX_1, OUTPUT);
  pinMode(AUDIO_MUX_2, OUTPUT);


  initEventSystem();
  initTCB0();
  initTCB1();
  initTCB3();

  setVideoPins();
  setAudioPins();
}

void loop() {

  getTimings();

  if(!settingsMenu)
    drawTimings();
  else
    drawDiagTimings();

  if (!videoPinsSet)
    setVideoPins();

  if (!audioPinsSet)
    setAudioPins();

  if(timingHSyncTimer.IsRunning){
    if (timingHSyncTimer.ElapsedTime() > 100) {
      timingHSyncTimer.IsRunning = false;
      tempHorizontalFreq = 0.0f;
      tempHorizontalPos = 0.0f;
      tempHorizontalNeg = 0.0f;
    }
  }
  if(timingVSyncTimer.IsRunning){
    if (timingVSyncTimer.ElapsedTime() > 100) {
      timingVSyncTimer.IsRunning = false;
      tempVerticalFreq = 0.0f;
      tempVerticalPos = 0.0f;
      tempVerticalNeg = 0.0f;
    }
  }
  if(timingOddEvenTimer.IsRunning){
    if (timingOddEvenTimer.ElapsedTime() > 100) {
      timingOddEvenTimer.IsRunning = false;
      tempInterlacedFreq = 0.0f;
      tempInterlacedPos = 0.0f;
      tempInterlacedNeg = 0.0f;
    }
  }

  if(But1.ElapsedTime() > 1000) {
    if(!settingsMenu) {
      settingsMenu = true;

      TFT.fillRect(0, 0, 320, 170, BG_COLOR);
      But1.Reset();
    }
  }

  if (But1.WasPressed()) {
    selectedPreset++;

    if(selectedPreset >= 9)
      selectedPreset = 0;

    switch(selectedPreset){
      case 0: { avSetting = 0b00110100 ; break;}
      case 1: { avSetting = 0b00110101 ; break;}
      case 2: { avSetting = 0b00110110 ; break;}
      case 3: { avSetting = 0b00011000 ; break;}
      case 4: { avSetting = 0b00011001 ; break;}
      case 5: { avSetting = 0b00011010 ; break;}
      case 6: { avSetting = 0b00011011 ; break;}
      case 7: { avSetting = 0b00101110 ; break;}
      case 8: { avSetting = 0b00101101 ; break;}      
    }

    videoPinsSet = false;
    audioPinsSet = false;
    
  }

  if (But2.WasPressed()) {
    int8_t videoSetting = (avSetting & 0b00001100) >> 2;
    avSetting = avSetting & 0b11110000;

    if (videoSetting == 1) {
      avSetting = avSetting | 0b00001000;
    } else if (videoSetting == 2) {
      avSetting = avSetting | 0b00001110;
    } else if (videoSetting == 3) {
      avSetting = avSetting | 0b00000100;
    } else if (videoSetting == 0) {
      avSetting = avSetting | 0b00000100;
    }
    videoPinsSet = false;
  }

  if (But3.WasPressed()) {
    int8_t videoSetting = (avSetting & 0b00001100) >> 2;
    int8_t formatSetting = avSetting & 0b00000011;
    avSetting = avSetting & 0b11111100;

    if (videoSetting == 1) {  //SCART
      if (formatSetting == 0) {
        avSetting = avSetting | 0b00000001;
      } else if (formatSetting == 1) {
        avSetting = avSetting | 0b00000010;
      } else if (formatSetting == 2) {
        avSetting = avSetting | 0b00000000;
      }
    } 
    else if (videoSetting == 2) {  //VGA
      if (formatSetting == 0) {
        avSetting = avSetting | 0b00000001;
      } else if (formatSetting == 1) {
        avSetting = avSetting | 0b00000010;
      } else if (formatSetting == 2) {
        avSetting = avSetting | 0b00000011;
      } else if (formatSetting == 3) {
        avSetting = avSetting | 0b00000000;
      }
    } 
    else if (videoSetting == 3) {  //Component
      if (formatSetting == 2) {
        avSetting = avSetting | 0b00000001;
      } else if (formatSetting == 1) {
        avSetting = avSetting | 0b00000010;
      }
    }

    videoPinsSet = false;
  }

  if (But4.WasPressed()) {
    if(!settingsMenu){
      int8_t audioSetting = (avSetting & 0b00110000) >> 4;
      avSetting = avSetting & 0b11001111;

      if (audioSetting == 3) {
        avSetting = avSetting | 0b00010000;
      } else if (audioSetting == 2) {
        avSetting = avSetting | 0b00110000;
      } else if (audioSetting == 1) {
        avSetting = avSetting | 0b00100000;
      } else if (audioSetting == 0) {
        avSetting = avSetting | 0b00110000;
      }

      audioPinsSet = false;
    }
    else {
      settingsMenu = false;
      printBackground();
      videoPinsSet = false;
      audioPinsSet = false;
    }
  }

  But1.Read();
  But2.Read();
  But3.Read();
  But4.Read();


  timingSamples[tSampleIndex].horizontal = tempHorizontalFreq;
  timingSamples[tSampleIndex].vertical = tempVerticalFreq;
  timingSamples[tSampleIndex].interlaced = tempInterlacedFreq;

  tSampleIndex++;
  
  if(tSampleIndex >= 10){

    uint16_t errorCount = 0;
    float averageHSync = timingSamples[0].horizontal;
    float averageVSync = timingSamples[0].vertical;
    float averageInterlaced = timingSamples[0].interlaced;


    for(int i = 1; i < 10; i++){
      if(timingSamples[i].horizontal > timingSamples[0].horizontal * 1.3f || timingSamples[i].horizontal < timingSamples[0].horizontal * (1.3f/(pow(1.3f, 2))))
        errorCount++;
      
      averageHSync += timingSamples[i].horizontal;
      averageVSync += timingSamples[i].vertical;
      averageInterlaced += timingSamples[i].interlaced;
    }


    if(errorCount == 0){
      timings.horizontal = averageHSync / 10;
      timings.vertical = averageVSync / 10;
      timings.interlaced = averageInterlaced / 10;

      if(timings.vertical != 0.0f){
        float linecount = floor(timings.horizontal * 1000 / timings.vertical);
        if (timings.interlaced != 0)
          timings.setLinecount(linecount * 2 + 1);
        else
          timings.setLinecount(linecount);
      }
      else{
        timings.setLinecount(0);
      }
    }

    if(errorCount >= 1 || settingsMenu)
      timings.setLock(false);
    else{
      timings.setLock(true);
    }

    tSampleIndex = 0;
  }



// Debug Code
  /*TFT.setTextColor(FG_COLOR, BG_COLOR);
  TFT.setTextSize(1);
  TFT.setCursor(0, 0);
  TFT.print(millis());
  TFT.setCursor(0, 8);
  TFT.print(tempHorizontal);
  TFT.setCursor(0, 16);
  TFT.print(tempVertical);
  TFT.setCursor(0, 24);
  TFT.print(tempInterlaced);

*/

}
