// (c) 2013 Artjom Eske
// This code is licensed under MIT license (see LICENSE.txt for details)
// Version 1.1 - Min HW: 1.3

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
#include "Settings.h"
#include "States.h"

// Hardware pin definitions
#define PIN_THS7374_BYPASS 24
#define PIN_THS7374_DISABLE 23

#define PIN_LMH1251_SELECT 21
#define PIN_LMH1251_DETECT 20
#define PIN_LMH1251_SDHD 19
#define PIN_LMH1251_POWERSAVE 18

#define PIN_MAX4887_1_ENABLE 14
#define PIN_MAX4887_1_SELECT 15

#define PIN_MAX4887_2_ENABLE 16
#define PIN_MAX4887_2_SELECT 17

#define PIN_TS5A3357_1 10
#define PIN_TS5A3357_2 11

#define PIN_LVC1G3_IN 7
#define PIN_LVC1G3_OUT 37

#define PIN_LMH1980_CVBS_FILTER 0
#define PIN_LMH1980_CVBS_HD 0

#define PIN_LMH1980_SOGY_FILTER 13
#define PIN_LMH1980_SOGY_HD 12

#define PIN_LMH1980_MCU_HD 0

#define PIN_MAX4908_CB1 25
#define PIN_MAX4908_CB2 26

#define PIN_BUTTON_PRESET 0
#define PIN_BUTTON_VIDEO 1
#define PIN_BUTTON_FORMAT 2
#define PIN_BUTTON_AUDIO 3

#define TESTPIN 32

// String and color definitions
#define FG_COLOR 0xFFFF
#define BG_COLOR 0x0000
#define T_COLOR 0x07E0
#define F_COLOR 0xF800
#define SL_COLOR 0xC409
#define FONT_W 6
#define FONT_H 8
#define HS_TEXT "HSync:"
#define VS_TEXT "VSync:"
#define VD_TEXT "Video: "
#define AD_TEXT "Audio: "
#define HZ_TEXT "Hz"
#define KHZ_TEXT "kHz"
#define MS_TEXT "ms"
#define PROGRESSIVE_TEXT "p"
#define INTERLACED_TEXT "i"
#define NO_SYNC_TEXT_1 "No"
#define NO_SYNC_TEXT_2 "Sync"


// Classes and structs

enum class DISPLAY_STATE {
	HOME,
	DIAG1,
	DIAG2
};

enum class VIDEO_INPUT {
	SCART,
	VGA,
	COMPONENT
};

enum class VIDEO_FORMAT {
	RGBS,
	RGsB,
	YPbPr,
	RGBHV
};

enum class AUDIO_INPUT {
	SCART,
	AUX,
	RCA,
};


// Fields

DISPLAY_STATE displayState = DISPLAY_STATE::HOME;
VIDEO_INPUT videoInput = VIDEO_INPUT::VGA;
VIDEO_FORMAT videoFormat = VIDEO_FORMAT::YPbPr;
AUDIO_INPUT audioInput = AUDIO_INPUT::AUX;
States states;
States lastStates;
Settings settings;

SoftwareTimer standbyTimer;

uint64_t loopStart = 0;

Button ButP(PIN_BUTTON_PRESET);
Button ButV(PIN_BUTTON_VIDEO);
Button ButF(PIN_BUTTON_FORMAT);
Button ButA(PIN_BUTTON_AUDIO);

Adafruit_ST7789 TFT = Adafruit_ST7789(33, 38, 39);

SoftwareTimer timingHSyncTimer;
SoftwareTimer timingVSyncTimer;
SoftwareTimer timingOddEvenTimer;

HardwareTimer<0> timerTCB0;
HardwareTimer<1> timerTCB1;
HardwareTimer<3> timerTCB3;

int8_t avSetting = 0b00110100;  // first nibble audio / last nibble video and format

bool pinsSet = false;
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

	setLinecount(int32_t count) {
		if (lineCount == count)
			return;

		lineCount = count;

		if (locked)
			printLinecount();

	}

	setLock(bool state) {

		if (locked == state)
			return;

		locked = state;

		if (!locked) {
			lineCount = 0;
			printLinecount();
		}
		else
		{
			printLinecount();
		}

	}

private:

	void centerText(int32_t width, int32_t height, int32_t& x, int32_t& y, int32_t fontSize, String str) {

		int32_t textWidth = strlen(str.c_str()) * 6 * fontSize;
		int32_t textHeight = 8 * fontSize;

		x = (width / 2 - textWidth / 2) + 1;
		y = height / 2 - textHeight / 2;

	}

	void printLinecount() {

		TFT.setTextColor(FG_COLOR);
		TFT.setTextSize(4);

		if (lineCount == 0 || !locked) {
			if (lastPrinted == NO_SYNC_TEXT_1)
				return;
		}
		else if (String(lineCount) == lastPrinted)
			return;

		TFT.fillRect(lastX, lastY, lastW, lastH, BG_COLOR);

		if (lineCount == 0 || !locked) {

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

		if (interlaced != 0.0f)
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

// HW Timer

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


void printScreen() {

	switch (displayState) {
		case DISPLAY_STATE::HOME:{
			// Draw blackscreen with white walls
			TFT.fillRect(0, 0, 320, 170, BG_COLOR);
			TFT.fillRect(164, 0, 3, 121, FG_COLOR);
			TFT.fillRect(0, 120, 320, 3, FG_COLOR);

			// Draw timing text in size 3
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

			// Draw placeholder timings
			TFT.setCursor(173, 36);
			TFT.print("00.00");
			TFT.setCursor(191, 93);
			TFT.print("00.00");

			// Draw current input text
			TFT.setTextSize(2);

			TFT.setCursor(6, 129);
			TFT.print(VD_TEXT);
			TFT.setCursor(6, 149);
			TFT.print(AD_TEXT);

			break;
		}
		case DISPLAY_STATE::DIAG1:{
			TFT.fillRect(0, 0, 320, 170, BG_COLOR);
			TFT.fillRect(159, 5, 2, 150, FG_COLOR);
			TFT.setTextColor(FG_COLOR, BG_COLOR);
			TFT.setTextSize(1);

			{ // MCU
				TFT.fillRect(5, 8, 148, 2, FG_COLOR);
				TFT.fillRect(65, 5, 28, 10, BG_COLOR);
				TFT.setCursor(70, 5);
				TFT.print("MCU");

				TFT.setCursor(5, 15);
				TFT.print("Time:");
				TFT.setCursor(5, 25);
				TFT.print("Loop:");
			}

			{ // VIDEO
				TFT.fillRect(5, 38, 148, 2, FG_COLOR);
				TFT.fillRect(59, 35, 40, 10, BG_COLOR);
				TFT.setCursor(64, 35);
				TFT.print("VIDEO");

				TFT.setCursor(5, 45);
				TFT.print("H. Sync:");
				TFT.setCursor(5, 55);
				TFT.print("V. Sync:");
				TFT.setCursor(5, 65);
				TFT.print("Interlaced:");
			}

			{ // LMH1251
				TFT.fillRect(5, 78, 148, 2, FG_COLOR);
				TFT.fillRect(54, 75, 52, 10, BG_COLOR);
				TFT.setCursor(59, 75);
				TFT.print("LMH1251");

				TFT.setCursor(5, 85);
				TFT.print("Selected:");
				TFT.setCursor(5, 95);
				TFT.print("Auto/Manual:");
				TFT.setCursor(5, 105);
				TFT.print("HD:");
				TFT.setCursor(5, 115);
				TFT.print("Powersaver:");
			}

			{ // THS7374
				TFT.fillRect(5, 128, 148, 2, FG_COLOR);
				TFT.fillRect(54, 125, 52, 10, BG_COLOR);
				TFT.setCursor(59, 125);
				TFT.print("THS7374");

				TFT.setCursor(5, 135);
				TFT.print("Disabled:");
				TFT.setCursor(5, 145);
				TFT.print("Bypass:");
			}

			{ // MAX4887 1
				TFT.fillRect(167, 8, 148, 2, FG_COLOR);
				TFT.fillRect(209, 5, 64, 10, BG_COLOR);
				TFT.setCursor(214, 5);
				TFT.print("MAX4887 1");

				TFT.setCursor(167, 15);
				TFT.print("Selected:");
				TFT.setCursor(167, 25);
				TFT.print("Enabled:");
			}

			{ // MAX4887 2
				TFT.fillRect(167, 38, 148, 2, FG_COLOR);
				TFT.fillRect(209, 35, 64, 10, BG_COLOR);
				TFT.setCursor(214, 35);
				TFT.print("MAX4887 2");

				TFT.setCursor(167, 45);
				TFT.print("Selected:");
				TFT.setCursor(167, 55);
				TFT.print("Enabled:");
			}

			{ // TS5A3357
				TFT.fillRect(167, 68, 148, 2, FG_COLOR);
				TFT.fillRect(212, 65, 58, 10, BG_COLOR);
				TFT.setCursor(217, 65);
				TFT.print("TS5A3357");

				TFT.setCursor(167, 75);
				TFT.print("Selected:");
			}

			{ // SN74LVC1G3157 1
				TFT.fillRect(167, 88, 148, 2, FG_COLOR);
				TFT.fillRect(191, 85, 100, 10, BG_COLOR);
				TFT.setCursor(196, 85);
				TFT.print("SN74LVC1G3157 1");

				TFT.setCursor(167, 95);
				TFT.print("Selected:");
			}

			{ // SN74LVC1G3157 2
				TFT.fillRect(167, 108, 148, 2, FG_COLOR);
				TFT.fillRect(191, 105, 100, 10, BG_COLOR);
				TFT.setCursor(196, 105);
				TFT.print("SN74LVC1G3157 2");

				TFT.setCursor(167, 115);
				TFT.print("Selected:");
			}

			{ // MAX4908
				TFT.fillRect(167, 128, 148, 2, FG_COLOR);
				TFT.fillRect(215, 125, 52, 10, BG_COLOR);
				TFT.setCursor(220, 125);
				TFT.print("MAX4908");

				TFT.setCursor(167, 135);
				TFT.print("Selected:");
			}
		

			TFT.fillRect(0, 160, 320, 10, FG_COLOR);
			TFT.setTextColor(BG_COLOR, FG_COLOR);
			TFT.setTextSize(1);
			TFT.setCursor(100, 161);
			TFT.print("Diagnostics Page 1/2");

			break;
		}
		case DISPLAY_STATE::DIAG2: {

			TFT.fillRect(0, 0, 320, 170, BG_COLOR);
			TFT.fillRect(159, 5, 2, 150, FG_COLOR);
			TFT.setTextColor(FG_COLOR, BG_COLOR);
			TFT.setTextSize(1);

			{ //LMH1980 CVBS
				TFT.fillRect(5, 8, 148, 2, FG_COLOR);
				TFT.fillRect(38, 5, 82, 10, BG_COLOR);
				TFT.setCursor(43, 5);
				TFT.print("LMH1980 CVBS");

				TFT.setCursor(5, 15);
				TFT.print("Filter:");
				TFT.setCursor(5, 25);
				TFT.print("HD:");
			}

			{ //LMH1980 SOG/Y
				TFT.fillRect(5, 38, 148, 2, FG_COLOR);
				TFT.fillRect(35, 35, 88, 10, BG_COLOR);
				TFT.setCursor(40, 35);
				TFT.print("LMH1980 SOG/Y");

				TFT.setCursor(5, 45);
				TFT.print("Filter:");
				TFT.setCursor(5, 55);
				TFT.print("HD:");
			}

			{ //LMH1980 MCU
				TFT.fillRect(5, 68, 148, 2, FG_COLOR);
				TFT.fillRect(41, 65, 76, 10, BG_COLOR);
				TFT.setCursor(46, 65);
				TFT.print("LMH1980 MCU");

				TFT.setCursor(5, 75);
				TFT.print("HD:");
			}

			TFT.fillRect(0, 160, 320, 10, FG_COLOR);
			TFT.setTextColor(BG_COLOR, FG_COLOR);
			TFT.setTextSize(1);
			TFT.setCursor(100, 161);
			TFT.print("Diagnostics Page 2/2");
			break;
		}
	}

}

void printInt32AsString(int32_t data, int32_t lastData, String suffix, int x, int y, int color, int backcolor, bool clearBack, int size) {

	String old = String(lastData);
	old.concat(suffix);
	int width = old.length() * FONT_W * size;
	
	if(clearBack)
		TFT.fillRect(x, y, width, size * FONT_H, backcolor);

	TFT.setCursor(x, y);
	TFT.setTextColor(color, backcolor);

	TFT.print(data + suffix);
}

void printFloatAsString(float data, float lastData, int32_t places, String suffix, int x, int y, int color, int backcolor, bool clearBack, int size) {

	String old = String(lastData);
	int width = old.length() * FONT_W * size;

	String str = String(data);
	int dotIndex = str.indexOf('.');
	str = str.substring(0, dotIndex + places + 1);

	if (clearBack)
		TFT.fillRect(x, y, width, size * FONT_H, backcolor);

	TFT.setCursor(x, y);
	TFT.setTextColor(color, backcolor);

	TFT.print(str + suffix);
}

void printData() {
	switch (displayState) {
		case DISPLAY_STATE::HOME:{
			break;
		}
		case DISPLAY_STATE::DIAG1:{
			//Print white text
			TFT.setTextColor(FG_COLOR, BG_COLOR);
			TFT.setTextSize(1);
		
			{ // MCU
				printInt32AsString(millis(), millis(), MS_TEXT, 79, 15, FG_COLOR, BG_COLOR, false, 1);

				if (states.MCU_LOOP != lastStates.MCU_LOOP) {
					printInt32AsString(states.MCU_LOOP, lastStates.MCU_LOOP, MS_TEXT, 79, 25, FG_COLOR, BG_COLOR, true, 1);
				}
			}
		
			{ // VIDEO
				if (states.H_FRQ != lastStates.H_FRQ) {
					printFloatAsString(states.H_FRQ, lastStates.H_FRQ, 2, KHZ_TEXT, 79, 45, FG_COLOR, BG_COLOR, true, 1);
				}
		
				if (states.V_FRQ != lastStates.V_FRQ) {
					printFloatAsString(states.V_FRQ, lastStates.V_FRQ, 2, HZ_TEXT, 79, 55, FG_COLOR, BG_COLOR, true, 1);
				}
		
				if (states.O_FRQ != lastStates.O_FRQ) {
					if (states.O_FRQ != 0.0f) {
						TFT.fillRect(79, 65, 12, 10, BG_COLOR);
						TFT.setCursor(79, 65);
						TFT.setTextColor(FG_COLOR);
						TFT.print("Yes");
					}
					else {
						TFT.fillRect(79, 65, 18, 10, BG_COLOR);
						TFT.setCursor(79, 65);
						TFT.setTextColor(FG_COLOR);
						TFT.print("No");
					}
				}
			}

			{ // LMH1251
				if (states.LMH1251_SELECT && states.LMH1251_SELECT != lastStates.LMH1251_SELECT) {
					TFT.fillRect(79, 85, 18, 10, BG_COLOR);
					TFT.setCursor(79, 85);
					TFT.setTextColor(FG_COLOR);
					TFT.print("YPbPr");
				}
				else if (!states.LMH1251_SELECT && states.LMH1251_SELECT != lastStates.LMH1251_SELECT) {
					TFT.fillRect(79, 85, 30, 10, BG_COLOR);
					TFT.setCursor(79, 85);
					TFT.setTextColor(FG_COLOR);
					TFT.print("RGB");
				}
		
				if (states.LMH1251_AUTOMANUAL && states.LMH1251_AUTOMANUAL != lastStates.LMH1251_AUTOMANUAL) {
					TFT.fillRect(79, 95, 36, 10, BG_COLOR);
					TFT.setCursor(79, 95);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Auto");
				}
				else if (!states.LMH1251_AUTOMANUAL && states.LMH1251_AUTOMANUAL != lastStates.LMH1251_AUTOMANUAL) {
					TFT.fillRect(79, 95, 24, 10, BG_COLOR);
					TFT.setCursor(79, 95);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Manual");
				}
		
				if (states.LMH1251_SDHD && states.LMH1251_SDHD != lastStates.LMH1251_SDHD) {
					TFT.fillRect(79, 105, 12, 10, BG_COLOR);
					TFT.setCursor(79, 105);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.LMH1251_SDHD && states.LMH1251_SDHD != lastStates.LMH1251_SDHD) {
					TFT.fillRect(79, 105, 18, 10, BG_COLOR);
					TFT.setCursor(79, 105);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
		
				if (states.LMH1251_POWERSAVER && states.LMH1251_POWERSAVER != lastStates.LMH1251_POWERSAVER) {
					TFT.fillRect(79, 115, 48, 10, BG_COLOR);
					TFT.setCursor(79, 115);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Enabled");
				}
				else if (!states.LMH1251_POWERSAVER && states.LMH1251_POWERSAVER != lastStates.LMH1251_POWERSAVER) {
					TFT.fillRect(79, 115, 42, 10, BG_COLOR);
					TFT.setCursor(79, 115);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Disabled");
				}
			}

			{ // THS7374
				if (states.THS7374_DISABLE && states.THS7374_DISABLE != lastStates.THS7374_DISABLE) {
					TFT.fillRect(79, 135, 12, 10, BG_COLOR);
					TFT.setCursor(79, 135);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.THS7374_DISABLE && states.THS7374_DISABLE != lastStates.THS7374_DISABLE) {
					TFT.fillRect(79, 135, 18, 10, BG_COLOR);
					TFT.setCursor(79, 135);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
		
				if (states.THS7374_FILTERBYPASS && states.THS7374_FILTERBYPASS != lastStates.THS7374_FILTERBYPASS) {
					TFT.fillRect(79, 145, 12, 10, BG_COLOR);
					TFT.setCursor(79, 145);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.THS7374_FILTERBYPASS && states.THS7374_FILTERBYPASS != lastStates.THS7374_FILTERBYPASS) {
					TFT.fillRect(79, 145, 18, 10, BG_COLOR);
					TFT.setCursor(79, 145);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
			}

			{ // MAX4887 1
				if (states.MAX4887_1_SELECT && states.MAX4887_1_SELECT != lastStates.MAX4887_1_SELECT) {
					TFT.fillRect(241, 15, 6, 10, BG_COLOR);
					TFT.setCursor(241, 15);
					TFT.setTextColor(FG_COLOR);
					TFT.print("2");
				}
				else if (!states.MAX4887_1_SELECT && states.MAX4887_1_SELECT != lastStates.MAX4887_1_SELECT) {
					TFT.fillRect(241, 15, 6, 10, BG_COLOR);
					TFT.setCursor(241, 15);
					TFT.setTextColor(FG_COLOR);
					TFT.print("1");
				}

				if (states.MAX4887_1_ENABLE && states.MAX4887_1_ENABLE != lastStates.MAX4887_1_ENABLE) {
					TFT.fillRect(241, 25, 12, 10, BG_COLOR);
					TFT.setCursor(241, 25);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.MAX4887_1_ENABLE && states.MAX4887_1_ENABLE != lastStates.MAX4887_1_ENABLE) {
					TFT.fillRect(241, 25, 18, 10, BG_COLOR);
					TFT.setCursor(241, 25);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
			}

			{ // MAX4887 2
				if (states.MAX4887_1_SELECT && states.MAX4887_1_SELECT != lastStates.MAX4887_1_SELECT) {
					TFT.fillRect(241, 45, 6, 10, BG_COLOR);
					TFT.setCursor(241, 45);
					TFT.setTextColor(FG_COLOR);
					TFT.print("2");
				}
				else if (!states.MAX4887_1_SELECT && states.MAX4887_1_SELECT != lastStates.MAX4887_1_SELECT) {
					TFT.fillRect(241, 45, 6, 10, BG_COLOR);
					TFT.setCursor(241, 45);
					TFT.setTextColor(FG_COLOR);
					TFT.print("1");
				}

				if (states.MAX4887_1_ENABLE && states.MAX4887_1_ENABLE != lastStates.MAX4887_1_ENABLE) {
					TFT.fillRect(241, 55, 12, 10, BG_COLOR);
					TFT.setCursor(241, 55);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.MAX4887_1_ENABLE && states.MAX4887_1_ENABLE != lastStates.MAX4887_1_ENABLE) {
					TFT.fillRect(241, 55, 18, 10, BG_COLOR);
					TFT.setCursor(241, 55);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
			}

			{ // TS5A3357

				if (states.TS5A3357_1 != lastStates.TS5A3357_1 || states.TS5A3357_2 != lastStates.TS5A3357_2) {

					if (states.TS5A3357_1 && states.TS5A3357_2) {
						TFT.fillRect(241, 75, 42, 10, BG_COLOR);
						TFT.setCursor(241, 75);
						TFT.setTextColor(FG_COLOR);
						TFT.print("Green");
					}
					else if (states.TS5A3357_1 && !states.TS5A3357_2) {
						TFT.fillRect(241, 75, 42, 10, BG_COLOR);
						TFT.setCursor(241, 75);
						TFT.setTextColor(FG_COLOR);
						TFT.print("CVBS");
					}
					else if (!states.TS5A3357_1 && states.TS5A3357_2) {
						TFT.fillRect(241, 75, 42, 10, BG_COLOR);
						TFT.setCursor(241, 75);
						TFT.setTextColor(FG_COLOR);
						TFT.print("H/VSync");
					}
					else {
						TFT.fillRect(241, 75, 42, 10, BG_COLOR);
						TFT.setCursor(241, 75);
						TFT.setTextColor(FG_COLOR);
						TFT.print("Off");
					}

				}
			}
		
			{ //SN74LVC1G3157 1
				if (states.LVC1G3_IN && states.LVC1G3_IN != lastStates.LVC1G3_IN) {
					TFT.fillRect(241, 95, 6, 10, BG_COLOR);
					TFT.setCursor(241, 95);
					TFT.setTextColor(FG_COLOR);
					TFT.print("2");
				}
				else if (!states.LVC1G3_IN && states.LVC1G3_IN != lastStates.LVC1G3_IN) {
					TFT.fillRect(241, 95, 6, 10, BG_COLOR);
					TFT.setCursor(241, 95);
					TFT.setTextColor(FG_COLOR);
					TFT.print("1");
				}
			}

			{ //SN74LVC1G3157 2
				if (states.LVC1G3_OUT && states.LVC1G3_OUT != lastStates.LVC1G3_OUT) {
					TFT.fillRect(241, 115, 6, 10, BG_COLOR);
					TFT.setCursor(241, 115);
					TFT.setTextColor(FG_COLOR);
					TFT.print("2");
				}
				else if (!states.LVC1G3_OUT && states.LVC1G3_OUT != lastStates.LVC1G3_OUT) {
					TFT.fillRect(241, 115, 6, 10, BG_COLOR);
					TFT.setCursor(241, 115);
					TFT.setTextColor(FG_COLOR);
					TFT.print("1");
				}
			}

			{ // MAX4908

				if (states.MAX4908_CB1 != lastStates.MAX4908_CB1 || states.MAX4908_CB2 != lastStates.MAX4908_CB2) {

					if (states.MAX4908_CB1 && states.MAX4908_CB2) {
						TFT.fillRect(241, 135, 30, 10, BG_COLOR);
						TFT.setCursor(241, 135);
						TFT.setTextColor(FG_COLOR);
						TFT.print("SCART");
					}
					else if (states.MAX4908_CB1 && !states.MAX4908_CB2) {
						TFT.fillRect(241, 135, 30, 10, BG_COLOR);
						TFT.setCursor(241, 135);
						TFT.setTextColor(FG_COLOR);
						TFT.print("AUX");
					}
					else if (!states.MAX4908_CB1 && states.MAX4908_CB2) {
						TFT.fillRect(241, 135, 30, 10, BG_COLOR);
						TFT.setCursor(241, 135);
						TFT.setTextColor(FG_COLOR);
						TFT.print("RCA");
					}
					else {
						TFT.fillRect(241, 135, 30, 10, BG_COLOR);
						TFT.setCursor(241, 135);
						TFT.setTextColor(FG_COLOR);
						TFT.print("Off");
					}

				}
			}
			break;
		}
		case DISPLAY_STATE::DIAG2: {
		
			{ //LMH1980 CVBS

				if (states.LMH1980_CVBS_FILTER && states.LMH1980_CVBS_FILTER != lastStates.LMH1980_CVBS_FILTER) {
					TFT.fillRect(79, 15, 12, 10, BG_COLOR);
					TFT.setCursor(79, 15);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.LMH1980_CVBS_FILTER && states.LMH1980_CVBS_FILTER != lastStates.LMH1980_CVBS_FILTER) {
					TFT.fillRect(79, 15, 18, 10, BG_COLOR);
					TFT.setCursor(79, 15);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}

				if (states.LMH1980_CVBS_HD && states.LMH1980_CVBS_HD != lastStates.LMH1980_CVBS_HD) {
					TFT.fillRect(79, 25, 12, 10, BG_COLOR);
					TFT.setCursor(79, 25);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.LMH1980_CVBS_HD && states.LMH1980_CVBS_HD != lastStates.LMH1980_CVBS_HD) {
					TFT.fillRect(79, 25, 18, 10, BG_COLOR);
					TFT.setCursor(79, 25);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
			}

			{ //LMH1980 SOG/Y

				if (states.LMH1980_SOGY_FILTER && states.LMH1980_SOGY_FILTER != lastStates.LMH1980_SOGY_FILTER) {
					TFT.fillRect(79, 45, 12, 10, BG_COLOR);
					TFT.setCursor(79, 45);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.LMH1980_SOGY_FILTER && states.LMH1980_SOGY_FILTER != lastStates.LMH1980_SOGY_FILTER) {
					TFT.fillRect(79, 45, 18, 10, BG_COLOR);
					TFT.setCursor(79, 45);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}

				if (states.LMH1980_SOGY_HD && states.LMH1980_SOGY_HD != lastStates.LMH1980_SOGY_HD) {
					TFT.fillRect(79, 55, 12, 10, BG_COLOR);
					TFT.setCursor(79, 55);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.LMH1980_SOGY_HD && states.LMH1980_SOGY_HD != lastStates.LMH1980_SOGY_HD) {
					TFT.fillRect(79, 55, 18, 10, BG_COLOR);
					TFT.setCursor(79, 55);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
			}

			{ //LMH1980 MCU

				if (states.LMH1980_MCU_HD && states.LMH1980_MCU_HD != lastStates.LMH1980_MCU_HD) {
					TFT.fillRect(79, 75, 12, 10, BG_COLOR);
					TFT.setCursor(79, 75);
					TFT.setTextColor(FG_COLOR);
					TFT.print("Yes");
				}
				else if (!states.LMH1980_MCU_HD && states.LMH1980_MCU_HD != lastStates.LMH1980_MCU_HD) {
					TFT.fillRect(79, 75, 18, 10, BG_COLOR);
					TFT.setCursor(79, 75);
					TFT.setTextColor(FG_COLOR);
					TFT.print("No");
				}
			}

			break;
	}
	}

	lastStates = states;
	lastStates.Init = true;
}

void initLastState() {

	if (!lastStates.Init) {
		lastStates.LMH1251_SELECT = !states.LMH1251_SELECT;
		lastStates.LMH1251_AUTOMANUAL = !states.LMH1251_AUTOMANUAL;
		lastStates.LMH1251_SDHD = !states.LMH1251_SDHD;
		lastStates.LMH1251_POWERSAVER = !states.LMH1251_POWERSAVER;

		lastStates.THS7374_DISABLE = !states.THS7374_DISABLE;
		lastStates.THS7374_FILTERBYPASS = !states.THS7374_FILTERBYPASS;

		lastStates.MAX4887_1_SELECT = !states.MAX4887_1_SELECT;
		lastStates.MAX4887_1_ENABLE = !states.MAX4887_1_ENABLE;

		lastStates.MAX4887_2_SELECT = !states.MAX4887_2_SELECT;
		lastStates.MAX4887_2_ENABLE = !states.MAX4887_2_ENABLE;

		lastStates.MAX4908_CB1 = !states.MAX4908_CB1;
		lastStates.MAX4908_CB2 = !states.MAX4908_CB2;

		lastStates.LMH1980_CVBS_FILTER = !states.LMH1980_CVBS_FILTER;
		lastStates.LMH1980_CVBS_HD = !states.LMH1980_CVBS_HD;

		lastStates.LMH1980_SOGY_FILTER = !states.LMH1980_SOGY_FILTER;
		lastStates.LMH1980_SOGY_HD = !states.LMH1980_SOGY_HD;

		lastStates.LMH1980_MCU_HD = !states.LMH1980_MCU_HD;

		lastStates.LVC1G3_IN = !states.LVC1G3_IN;
		lastStates.LVC1G3_OUT = !states.LVC1G3_OUT;

		lastStates.TS5A3357_1 = !states.TS5A3357_1;
		lastStates.TS5A3357_2 = !states.TS5A3357_2;

		lastStates.MCU_LOOP = -1;

		lastStates.H_FRQ = -1;
		lastStates.V_FRQ = -1;
		lastStates.O_FRQ = -1;

		lastStates.Init = true;
	}
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
		states.H_FRQ = 1 / (62.5 * 1.0 * (float)period / 1000000000UL) / 1000UL;
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
		states.V_FRQ = 1 / (62.5 * 64.0 * (float)period / 1000000000UL);
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
		states.O_FRQ = 1 / (62.5 * 64.0 * (float)period / 1000000000UL);
		timerTCB3.finish = false;

		timingOddEvenTimer.Start();
	}
}

/*
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
	TFT.print(horizontalFreq);
	TFT.setCursor(0, 16);
	TFT.print(horizontalPos);
	TFT.setCursor(0, 24);
	TFT.print(horizontalNeg);

	TFT.setCursor(0, 32);
	TFT.print(verticalFreq);
	TFT.setCursor(0, 40);
	TFT.print(verticalPos);
	TFT.setCursor(0, 48);
	TFT.print(verticalNeg);

	TFT.setCursor(0, 56);
	TFT.print(interlacedFreq);
	TFT.setCursor(0, 64);
	TFT.print(interlacedPos);
	TFT.setCursor(0, 72);
	TFT.print(interlacedNeg);

}
*/

void setPins() {
	
	switch (videoInput)
	{
	case VIDEO_INPUT::SCART: 
	{
		digitalWrite(PIN_MAX4887_1_SELECT, LOW);
		digitalWrite(PIN_MAX4887_1_ENABLE, LOW);

		digitalWrite(PIN_MAX4887_2_SELECT, LOW);
		digitalWrite(PIN_MAX4887_2_ENABLE, LOW);
		break;
	}
	case VIDEO_INPUT::VGA:
	{
		digitalWrite(PIN_MAX4887_1_SELECT, HIGH);
		digitalWrite(PIN_MAX4887_1_ENABLE, LOW);

		digitalWrite(PIN_MAX4887_2_SELECT, LOW);
		digitalWrite(PIN_MAX4887_2_ENABLE, LOW);
		break;
	}
		
	case VIDEO_INPUT::COMPONENT:
	{
		digitalWrite(PIN_MAX4887_1_SELECT, LOW);
		digitalWrite(PIN_MAX4887_1_ENABLE, HIGH);

		digitalWrite(PIN_MAX4887_2_SELECT, HIGH);
		digitalWrite(PIN_MAX4887_2_ENABLE, LOW);
		break;
	}
	default:
		break;
	}

	switch (videoFormat)
	{
	case VIDEO_FORMAT::RGBS: 
	{
		digitalWrite(PIN_LMH1251_SELECT, LOW);

		switch (videoInput)
		{
			case VIDEO_INPUT::SCART: 
			{
				digitalWrite(PIN_TS5A3357_1, HIGH);
				digitalWrite(PIN_TS5A3357_2, LOW);
				break;
			}
			case VIDEO_INPUT::VGA:
			{
				digitalWrite(PIN_TS5A3357_1, LOW);
				digitalWrite(PIN_TS5A3357_2, HIGH);

				digitalWrite(PIN_LVC1G3_IN, HIGH);
				digitalWrite(PIN_LVC1G3_OUT, LOW);
				break;
			}
		}
		break;
	}
		
	case VIDEO_FORMAT::RGsB:
	{
		digitalWrite(PIN_LMH1251_SELECT, LOW);

		digitalWrite(PIN_TS5A3357_1, HIGH);
		digitalWrite(PIN_TS5A3357_2, HIGH);

		break;
	}
	case VIDEO_FORMAT::YPbPr:
	{
		digitalWrite(PIN_LMH1251_SELECT, HIGH);

		digitalWrite(PIN_TS5A3357_1, HIGH);
		digitalWrite(PIN_TS5A3357_2, HIGH);
		break;
	}
	case VIDEO_FORMAT::RGBHV:
	{
		digitalWrite(PIN_LMH1251_SELECT, LOW);

		digitalWrite(PIN_TS5A3357_1, LOW);
		digitalWrite(PIN_TS5A3357_2, HIGH);
		
		digitalWrite(PIN_LVC1G3_IN, HIGH);
		digitalWrite(PIN_LVC1G3_OUT, LOW);
		break;
	}
	default:
		break;
	}

	switch (audioInput)
	{
	case AUDIO_INPUT::SCART:
	{
		digitalWrite(PIN_MAX4908_CB1, HIGH);
		digitalWrite(PIN_MAX4908_CB2, HIGH);
		break;
	}
		
	case AUDIO_INPUT::AUX:
	{
		digitalWrite(PIN_MAX4908_CB1, HIGH);
		digitalWrite(PIN_MAX4908_CB2, LOW);
		break;
	}
	case AUDIO_INPUT::RCA:
	{
		digitalWrite(PIN_MAX4908_CB1, LOW);
		digitalWrite(PIN_MAX4908_CB2, HIGH);
		break;
	}
	default:
		break;
	}

	pinsSet = true;
}

void disableOutput() {
	digitalWrite(PIN_LMH1251_POWERSAVE, HIGH);
	digitalWrite(PIN_THS7374_DISABLE, HIGH);

	digitalWrite(PIN_MAX4908_CB1, LOW);
	digitalWrite(PIN_MAX4908_CB2, LOW);
}
		
void enableOutput() {
	digitalWrite(PIN_LMH1251_POWERSAVE, LOW); 
	digitalWrite(PIN_THS7374_DISABLE, LOW);
	setPins();
}

void setup() {

	// Setup display
	TFT.init(170, 320, 0);
	TFT.setRotation(1);
	TFT.setTextWrap(false);

	// Setup IO
	pinMode(PIN_BUTTON_PRESET, INPUT);
	pinMode(PIN_BUTTON_VIDEO, INPUT);
	pinMode(PIN_BUTTON_FORMAT, INPUT);
	pinMode(PIN_BUTTON_AUDIO, INPUT);
	pinMode(PIN_LVC1G3_IN, OUTPUT);
	pinMode(PIN_LVC1G3_OUT, OUTPUT);
	pinMode(PIN_THS7374_BYPASS, OUTPUT);
	pinMode(PIN_THS7374_DISABLE, OUTPUT);
	pinMode(PIN_TS5A3357_1, OUTPUT);
	pinMode(PIN_TS5A3357_2, OUTPUT);
	pinMode(PIN_LMH1251_SELECT, OUTPUT);
	pinMode(PIN_LMH1251_DETECT, OUTPUT);
	pinMode(PIN_LMH1251_SDHD, INPUT);
	pinMode(PIN_LMH1251_POWERSAVE, OUTPUT);
	pinMode(PIN_MAX4887_1_ENABLE, OUTPUT);
	pinMode(PIN_MAX4887_2_ENABLE, OUTPUT);
	pinMode(PIN_MAX4887_1_SELECT, OUTPUT);
	pinMode(PIN_MAX4887_2_SELECT, OUTPUT);
	pinMode(PIN_MAX4908_CB1, OUTPUT);
	pinMode(PIN_MAX4908_CB2, OUTPUT);
	pinMode(PIN_LMH1980_SOGY_FILTER, OUTPUT);

	pinMode(TESTPIN, INPUT);

	digitalWrite(PIN_LMH1251_DETECT, HIGH);

	// Setup hardware timers
	initEventSystem();
	initTCB0();
	initTCB1();
	initTCB3();


	setPins();

	// Print home screen
	printScreen();
}

void loop() {

	loopStart = millis();

	{ // Fetch all pin states and timings
		ButP.Read();
		ButV.Read();
		ButF.Read();
		ButA.Read();

		states.LMH1251_SELECT = digitalRead(PIN_LMH1251_SELECT);
		states.LMH1251_AUTOMANUAL = digitalRead(PIN_LMH1251_DETECT);
		states.LMH1251_SDHD = digitalRead(PIN_LMH1251_SDHD);
		states.LMH1251_POWERSAVER = digitalRead(PIN_LMH1251_POWERSAVE);

		states.THS7374_DISABLE = digitalRead(PIN_THS7374_DISABLE);
		states.THS7374_FILTERBYPASS = digitalRead(PIN_THS7374_BYPASS);

		states.MAX4887_1_SELECT = digitalRead(PIN_MAX4887_1_SELECT);
		states.MAX4887_1_ENABLE = digitalRead(PIN_MAX4887_1_ENABLE);

		states.MAX4887_2_SELECT = digitalRead(PIN_MAX4887_2_SELECT);
		states.MAX4887_2_ENABLE = digitalRead(PIN_MAX4887_2_ENABLE);

		states.MAX4908_CB1 = digitalRead(PIN_MAX4908_CB1);
		states.MAX4908_CB2 = digitalRead(PIN_MAX4908_CB2);

		states.LMH1980_CVBS_FILTER = digitalRead(PIN_LMH1980_CVBS_FILTER);
		states.LMH1980_CVBS_HD = digitalRead(PIN_LMH1980_CVBS_HD);

		states.LMH1980_SOGY_FILTER = digitalRead(PIN_LMH1980_SOGY_FILTER);
		states.LMH1980_SOGY_HD = !digitalRead(PIN_LMH1980_SOGY_HD);

		states.LMH1980_MCU_HD = digitalRead(PIN_LMH1980_MCU_HD);

		states.LVC1G3_IN = digitalRead(PIN_LVC1G3_IN);
		states.LVC1G3_OUT = digitalRead(PIN_LVC1G3_OUT);

		states.TS5A3357_1 = digitalRead(PIN_TS5A3357_1);
		states.TS5A3357_2 = digitalRead(PIN_TS5A3357_2);

		getTimings();
		initLastState();
	}

	{ // Print data to screen

		printData();

		if (timingHSyncTimer.IsRunning) {
			if (timingHSyncTimer.ElapsedTime() > 100) {
				timingHSyncTimer.IsRunning = false;
				states.H_FRQ = 0.0f;
			}
		}
		if (timingVSyncTimer.IsRunning) {
			if (timingVSyncTimer.ElapsedTime() > 100) {
				timingVSyncTimer.IsRunning = false;
				states.V_FRQ = 0.0f;
			}
		}
		if (timingOddEvenTimer.IsRunning) {
			if (timingOddEvenTimer.ElapsedTime() > 100) {
				timingOddEvenTimer.IsRunning = false;
				states.O_FRQ = 0.0f;
			}
		}


		if (ButP.ElapsedTime() > 500) {
			switch (displayState) {
			case DISPLAY_STATE::HOME: {
				displayState = DISPLAY_STATE::DIAG1;
				ButP.SetMissPress();
				lastStates.Init = false;
				printScreen();
				break;
			}
			default:
				break;
			}
		}

		if (ButP.WasPressed()) {

			switch (displayState) {
			case DISPLAY_STATE::HOME: {
				break;
			}
			case DISPLAY_STATE::DIAG1: {
				displayState = DISPLAY_STATE::DIAG2;
				lastStates.Init = false;
				printScreen();
				break;
			}
			case DISPLAY_STATE::DIAG2: {
				displayState = DISPLAY_STATE::DIAG1;
				lastStates.Init = false;
				printScreen();
				break;
			}
			default:
				break;
			}
		}

		if (ButA.WasPressed()) {
			switch (displayState)
			{
			case DISPLAY_STATE::HOME:
				break;
			case DISPLAY_STATE::DIAG1:
			case DISPLAY_STATE::DIAG2: {

				displayState = DISPLAY_STATE::HOME;
				printScreen();				break;
			}
			default:
				break;
			}
		}

		if (standbyTimer.IsRunning && standbyTimer.ElapsedTime() > 5000) {
			disableOutput();
			standbyTimer.Stop();
		}

		float linecount = floor(states.H_FRQ * 1000 / states.V_FRQ);
		if (states.O_FRQ != 0.0f)
			states.LINES = linecount * 2 + 1;
		else
			states.LINES = linecount;

		//TFT.setCursor(0, 0);
		//TFT.setTextColor(FG_COLOR, BG_COLOR);
		//TFT.print(states.LINES);


		digitalWrite(PIN_THS7374_BYPASS, HIGH);

		if ((states.LMH1980_SOGY_HD || states.LINES > 720) && (videoFormat == VIDEO_FORMAT::YPbPr || videoFormat == VIDEO_FORMAT::RGsB )) {
			digitalWrite(PIN_LMH1980_SOGY_FILTER, LOW);
			//digitalWrite(PIN_THS7374_BYPASS, HIGH);
		}
		else {
			digitalWrite(PIN_LMH1980_SOGY_FILTER, HIGH);
			//digitalWrite(PIN_THS7374_BYPASS, LOW);
		}


	}



	{ // Set all pin states

		if (!pinsSet)
			setPins();

		if (states.H_FRQ == 0.0f && states.V_FRQ == 0.0f) {
			if(!standbyTimer.IsRunning)
				standbyTimer.Start();

		}
		else {
			enableOutput();
		}

	}


	bool a = digitalRead(TESTPIN);
	TFT.setCursor(0, 0);
	TFT.setTextColor(FG_COLOR, BG_COLOR);
	TFT.print(a);

	states.MCU_LOOP = millis() - loopStart;



	/*

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

	  if(ButP.ElapsedTime() > 1000) {
		if(!settingsMenu) {
		  settingsMenu = true;

		  TFT.fillRect(0, 0, 320, 170, BG_COLOR);
		  ButP.Reset();
		}
	  }

	  if (ButP.WasPressed()) {
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

	  if (ButV.WasPressed()) {
		int8_t videoSetting = (avSetting & 0b00001100) >> 2;
		avSetting = avSetting & 0b11110000;

		switch(videoSetting){
		  case 0: { avSetting = avSetting | 0b00000100; break;}
		  case 1: { avSetting = avSetting | 0b00001000; break;}
		  case 2: { avSetting = avSetting | 0b00001110; break;}
		  case 3: { avSetting = avSetting | 0b00000100; break;}
		}

		videoPinsSet = false;
	  }

	  if (ButF.WasPressed()) {
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

	  if (ButA.WasPressed()) {
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

	  ButP.Read();
	  ButV.Read();
	  ButF.Read();
	  ButA.Read();


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

	  states.BUTTON_P = ButP.State();
	  states.BUTTON_V = ButV.State();
	  states.BUTTON_F = ButF.State();
	  states.BUTTON_A = ButA.State();



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
