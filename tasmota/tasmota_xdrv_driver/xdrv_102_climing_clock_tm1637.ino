#ifdef USE_CLIMBING_CLOCK_TM1637

#warning **** climbing clock tm1637 is included. ****
#define XDRV_102 102

#include <TM1637.h>
#include <ESP8266WiFi.h>
 
#ifdef CLK_PIN
#undef CLK_PIN
#endif

#ifdef DATA_PIN
#undef DATA_PIN
#endif

#define CLK_PIN 0
#define DATA_PIN 2

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

    int numbers;
};

void display_loading(void);
void display_clock(int offset);
void display_timer(time_t start, time_t top, time_t transition);
void display_numbers(char nums[4]);
void display(void);

TM1637 tm;

display_stat stat;
bool clock_init = false;
char dev_id[100];

void climbing_timer_setup(void) {
    snprintf(dev_id, sizeof(dev_id), MQTT_TOPIC, system_get_chip_id());

    pinMode(DATA_PIN, OUTPUT);
	pinMode(CLK_PIN, OUTPUT);

    tm.begin(CLK_PIN, DATA_PIN, 4);
    tm.setBrightness(7);
    tm.displayClear();
}

void display_loading(void) {
    tm.displayPChar("----");
}

void display_clock(int offset) {
    time_t now = time(nullptr) + offset;
    struct tm *t = localtime(&now);

    int hr = t->tm_hour;
    if (hr > 12)
        hr -= 12;

    tm.displayTime(hr, t->tm_min, false);
}

void display_timer(time_t start, time_t top, time_t transition) {
    int set = top + transition;
	int elapsed = time(nullptr) - start;

	int rem_sec = set - (elapsed % set);
    bool in_transition = rem_sec <= transition;

    if (elapsed < 0) {
        rem_sec = -elapsed;
    } else if (!in_transition) {
        rem_sec -= transition;
    }

    int h = rem_sec / 3600;
    int m = (rem_sec % 3600) / 60;
    int s = rem_sec % 60;

    if (h > 0)
        tm.displayTwoInt(h, m, false);
    else
        tm.displayTwoInt(m, s, false);
}

void display_numbers(int num) {
    tm.displayInt(num);
}

void display(void) {
    if (!clock_init) {
        display_loading();

        // TODO impl. local ntp server in case no internet
        if (MqttIsConnected()) {
            MqttPublishPayload("server/cmnd/clockinit", dev_id, strlen(dev_id), false);
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
        display_numbers(stat.numbers);
        break;
    }
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
    stat.numbers = n % 10000;

    ResponseCmndDone();
}

const char clk_commands[] PROGMEM = "|"  // No Prefix
  "clock|"
  "comptimer|"
  "numbers";

void (* const clk_command[])(void) PROGMEM = {
  &cmd_clock, &cmd_timer, &cmd_numbers };

bool Xdrv102(uint32_t function) {
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