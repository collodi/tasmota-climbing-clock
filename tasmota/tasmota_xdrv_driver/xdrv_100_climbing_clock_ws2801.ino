// TODO uncomment
//#ifdef USE_CLIMBING_CLOCK_WS2801

#warning **** climbing clock ws2801 is included. ****
#define XDRV_100 100

#include <FastLED.h>
#include <ESP8266WiFi.h>
 
#define NUM_LEDS 28
#define DATA_PIN 0
#define CLK_PIN 2

enum display_mode {
    CLOCK,
    TIMER,
    NUMBERS,
};

struct display_stat {
    display_mode mode = CLOCK;

    int clock_offset = 0;

    time_t timer_start = 0;
    time_t timer_top = 0;
    time_t timer_transition = 0;

    char numbers[4];
    CRGB num_colors[4];
};

void write_segment(int i, int seg, CRGB color);
void write_segments(int i, int segs, CRGB color);
void write_digit(int i, int n, CRGB color);
void display_loading(CRGB color);
void display_clock(int offset);
void display_timer(time_t start, time_t top, time_t transition);
void display_numbers(char nums[4], CRGB colors[4]);
void display(void);

CRGB leds[NUM_LEDS];

display_stat stat;

int segs_to_num[] = { 119, 68, 62, 110, 77, 107, 123, 70, 127, 111 };
bool clock_init = false;

void climbing_timer_setup(void) {
    pinMode(DATA_PIN, OUTPUT);
	pinMode(CLK_PIN, OUTPUT);

	FastLED.addLeds<WS2801, DATA_PIN, CLK_PIN, RGB>(leds, NUM_LEDS);
	FastLED.setBrightness(255);
	FastLED.clear();

    // TODO delete
    Serial.begin(115200);
}

void write_segment(int i, int seg, CRGB color) {
    leds[i * 7 + seg] = color;
}

/*
    i = digit index (0 - 3)
    segs = LSB -> 0th segment
*/
void write_segments(int i, int segs, CRGB color) {
    for (int j = 0; j < 7; j++) {
        if (segs & (1 << j))
            write_segment(i, j, color);
        else
            write_segment(i, j, CRGB::Black);
    }
}

/*
  i starts at 0 (digit index)
  n must be one of 0 - 9
*/
void write_digit(int i, int n, CRGB color) {
    write_segments(i, segs_to_num[n], color);
}

void display_loading(CRGB color) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    leds[3] = color;
    leds[10] = color;
    leds[17] = color;
    leds[24] = color;
}

void display_clock(int offset) {
    time_t now = time(nullptr) + offset;
    struct tm *t = localtime(&now);

    write_digit(0, t->tm_hour / 10, CRGB::Green);
    write_digit(1, t->tm_hour % 10, CRGB::Green);
    write_digit(2, t->tm_min / 10, CRGB::Green);
    write_digit(3, t->tm_min % 10, CRGB::Green);
}

void display_timer(time_t start, time_t top, time_t transition) {
    int set = top + transition;
	int elapsed = time(nullptr) - start;

    CRGB color = CRGB::Red;
	int rem_sec = set - (elapsed % set);
    bool in_transition = rem_sec <= transition;

    if (elapsed < 0) {
        color = CRGB::Yellow;
        rem_sec = -elapsed;
    } else if (!in_transition) {
        rem_sec -= transition;
    } else {
        color = CRGB::Green;
    }

    int h = rem_sec / 3600;
    int m = (rem_sec % 3600) / 60;
    int s = rem_sec % 60;

    if (h > 0) {
        write_digit(0, h / 10, color);
        write_digit(1, h % 10, color);
        write_digit(2, m / 10, color);
        write_digit(3, m % 10, color);
    } else {
        write_digit(0, m / 10, color);
        write_digit(1, m % 10, color);
        write_digit(2, s / 10, color);
        write_digit(3, s % 10, color);
    }
}

void display_numbers(char nums[4], CRGB colors[4]) {
    for (int i = 0; i < 4; i++)
        write_digit(i, nums[i], colors[i]);
}

void display(void) {
    if (!clock_init) {
        display_loading(CRGB::Yellow);
        FastLED.show();

        if (time(nullptr) > 1000000)
            clock_init = true;

        return;
    }

    switch (stat.mode) {
    case CLOCK:
        display_clock(stat.clock_offset);
        break;
    case TIMER:
        display_timer(stat.timer_start, stat.timer_top, stat.timer_transition);
        break;
    case NUMBERS:
        display_numbers(stat.numbers, stat.num_colors);
        break;
    }

	FastLED.show();
}

void cmd_clock(void) {
    stat.mode = CLOCK;
    stat.clock_offset = (int) strtol(XdrvMailbox.data, (char **) NULL, 10);

    ResponseCmndDone();
}

void cmd_timer(void) {
    stat.mode = TIMER;

    char *end;
    stat.timer_start = (int) strtol(XdrvMailbox.data, &end, 10);
    stat.timer_top = (int) strtol(end, &end, 10);
    stat.timer_transition = (int) strtol(end, &end, 10);

    ResponseCmndDone();
}

void cmd_numbers(void) {
    stat.mode = NUMBERS;

    char *end;

    int n = (int) strtol(XdrvMailbox.data, &end, 10);
    stat.numbers[0] = (n / 1000) % 10;
    stat.numbers[1] = (n / 100) % 10;
    stat.numbers[2] = (n / 10) % 10;
    stat.numbers[3] = n % 10;

    uint32_t c;
    for (int i = 0; i < 4; i++) {
        c = (uint32_t) strtol(end, &end, 16);
        memcpy(&(stat.num_colors[i]), &c, 3);
    }

    ResponseCmndDone();
}

const char clk_commands[] PROGMEM = "|"  // No Prefix
  "clock|"
  "comptimer|"
  "numbers";

void (* const clk_command[])(void) PROGMEM = {
  &cmd_clock, &cmd_timer, &cmd_numbers };

bool Xdrv100(uint32_t function) {
    bool result = false;

    switch (function) {
        case FUNC_INIT:
            climbing_timer_setup();
            break;
        case FUNC_EVERY_100_MSECOND:
            display();
            break;
        case FUNC_COMMAND:
            result = DecodeCommand(clk_commands, clk_command);
            break;
    }

    return result;
}

// TODO uncomment
//#endif