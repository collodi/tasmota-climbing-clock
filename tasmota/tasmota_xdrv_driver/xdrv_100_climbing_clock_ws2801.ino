#ifdef USE_CLIMBING_CLOCK_WS2801

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

void write_segments(int i, int segs, CRGB color);
inline void write_digit(int i, int n, CRGB color);
void write_two_nums(int n1, int n2, CRGB color);
void display_loading(CRGB color);
void display_clock(int offset);
void display_timer(time_t start, time_t top, time_t transition);
void display_numbers(char nums[4], CRGB colors[4]);
void display(void);

CRGB leds[NUM_LEDS];

display_stat stat;

int segs_to_num[] = { 119, 68, 62, 110, 77, 107, 123, 70, 127, 111 };
bool clock_init = false;

char dev_id[100];

void climbing_timer_setup(void) {
    snprintf(dev_id, sizeof(dev_id), MQTT_TOPIC, system_get_chip_id());

    pinMode(DATA_PIN, OUTPUT);
	pinMode(CLK_PIN, OUTPUT);

	FastLED.addLeds<WS2801, DATA_PIN, CLK_PIN, RGB>(leds, NUM_LEDS);
	FastLED.setBrightness(255);
	FastLED.clear();
}

/*
    i = digit index (0 - 3)
    segs = LSB -> 0th segment
*/
void write_segments(int i, int segs, CRGB color) {
    for (int j = 0; j < 7; j++) {
        if (segs & (1 << j))
            leds[i * 7 + j] = color;
        else
            leds[i * 7 + j] = CRGB::Black;
    }
}

/*
  i starts at 0 (digit index)
  n must be one of 0 - 9
*/
inline void write_digit(int i, int n, CRGB color) {
    write_segments(i, segs_to_num[n], color);
}

void write_two_nums(int n1, int n2, CRGB color) {
    write_digit(0, (n1 / 10) % 10, color);
    write_digit(1, n1 % 10, color);
    write_digit(2, (n2 / 10) % 10, color);
    write_digit(3, n2 % 10, color);
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
    write_two_nums(t->tm_hour, t->tm_min, CRGB::Green);
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

    if (h > 0)
        write_two_nums(h, m, color);
    else
        write_two_nums(m, s, color);
}

void display_numbers(char nums[4], CRGB colors[4]) {
    for (int i = 0; i < 4; i++)
        write_digit(i, nums[i], colors[i]);
}

void display(void) {
    // TODO what if no internet, but conn.d to wifi?
    if (!clock_init) {
        display_loading(CRGB::Yellow);
        FastLED.show();

        if (time(nullptr) > 1000000) {
            MqttPublishPayload("server/cmnd/clockoffset", dev_id, strlen(dev_id), false);
            clock_init = true;
        }

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
        case FUNC_EVERY_250_MSECOND:
            display();
            break;
        case FUNC_COMMAND:
            result = DecodeCommand(clk_commands, clk_command);
            break;
    }

    return result;
}

#endif