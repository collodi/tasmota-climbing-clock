#ifdef USE_CLIMBING_CLOCK_WS2801

#warning **** climbing clock ws2801 is included. ****
#define XDRV_100 100

#include <FastLED.h>

#define NUM_LEDS 28
#define DATA_PIN 0
#define CLK_PIN 2

CRGB leds[NUM_LEDS];

int idx = 0;
CRGB color = CRGB::Red;

void climbing_timer_setup(void) {
    pinMode(DATA_PIN, OUTPUT);
	pinMode(CLK_PIN, OUTPUT);

	FastLED.addLeds<WS2801, DATA_PIN, CLK_PIN, RGB>(leds, NUM_LEDS);
	FastLED.setBrightness(255);
	FastLED.clear();
}

void display(void) {
    leds[idx] = color;
    FastLED.show();

    idx += 1;
    if (idx == NUM_LEDS) {
        idx = 0;

        if (color == CRGB::Red)
            color = CRGB::Blue;
        else
            color = CRGB::Red;
    }


}

bool Xdrv100(uint32_t function) {
    bool result = false;

    switch (function) {
        case FUNC_INIT:
            climbing_timer_setup();
            break;
        case FUNC_EVERY_SECOND:
            display();
            break;
        case FUNC_COMMAND:
            //result = DecodeCommand(TTGO_Commands, TTTGO_Command);
            break;
    }

    return result;
}

#endif