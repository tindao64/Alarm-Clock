// Date and time functions using a PCF8523 RTC connected via I2C and Wire lib
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <DHT.h>

RTC_PCF8523 rtc;
LiquidCrystal_I2C lcd{0x27, 20, 4};
DHT dht;
#define BTN_LEFT A0
#define BTN_RIGHT A1
#define BTN_MINUS A2
#define BTN_PLUS A3
#define BUZZER 3


// Display State

#define MINUTES_PER_24H (60 * 24)
#define NUM_ALARMS 9
uint16_t alarms[NUM_ALARMS], now;

// 0 is no selection, then alarms
uint8_t selection;

// accounts for no selection
#define NUM_SELECTION ((2*NUM_ALARMS)+1)

unsigned long last_dht_sample;

void redraw_screen();
void selection_cursor();
void ringtone();

void setup() {
    Serial.begin(9600);
    dht.setup(8);
    lcd.init();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.backlight();

    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_MINUS, INPUT_PULLUP);
    pinMode(BTN_PLUS, INPUT_PULLUP);
    pinMode(BUZZER, OUTPUT);

    EEPROM.get(0, alarms);

    Serial.println("Done initializing");

    if (!rtc.begin()) {
        lcd.print("ERROR: No RTC!");
        Serial.println("No RTC!");
        while (1) delay(10);
    }
    if (!rtc.initialized() || rtc.lostPower()) {
    // if (true) {
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Setting RTC to:");
        Serial.println("Setting RTC");
        lcd.setCursor(0,1);
        lcd.print(F(__DATE__));
        lcd.setCursor(0,2);
        lcd.print(F(__TIME__));
        lcd.print(" (+6s)");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(6));
        rtc.start();
        delay(5000);
        lcd.clear();
        lcd.setCursor(0,0);
    }
    rtc.start();
}

void loop() {
    bool should_redraw = false;
    // buttons

    if (!digitalRead(BTN_LEFT)) {
        selection = (selection + NUM_SELECTION - 1) % NUM_SELECTION;
        should_redraw = true;
    }

    if (!digitalRead(BTN_RIGHT)) {
        selection = (selection + 1) % NUM_SELECTION;
        should_redraw = true;
    }

    if (!digitalRead(BTN_MINUS) && selection != 0) {
        uint8_t alarm = (selection - 1) / 2;
        uint8_t step = (selection % 2) ? 60 : 1;
        alarms[alarm] = (alarms[alarm] + MINUTES_PER_24H - step) % MINUTES_PER_24H;
        should_redraw = true;
    }

    if (!digitalRead(BTN_PLUS) && selection != 0) {
        uint8_t alarm = (selection - 1) / 2;
        uint8_t step = (selection % 2) ? 60 : 1;
        alarms[alarm] = (alarms[alarm] + step) % MINUTES_PER_24H;
        should_redraw = true;
    }

    // alarms

    DateTime time_now = rtc.now();
    uint16_t prev_now = now;
    now = time_now.hour() * 60 + time_now.minute();
    if (time_now.second() <= 2) {
        for (uint16_t alarm : alarms) {
            if (alarm == now && alarm != 0) {
                ringtone();
            }
        }
    }

    if (now != prev_now) {
        should_redraw = true;
    }

    if (should_redraw) {
        redraw_screen();
        EEPROM.put(0, alarms);
    }

    unsigned long millis_now = millis();
    if (millis_now - last_dht_sample >= 3000) {
        lcd.setCursor(8,0);
        float temp = dht.getTemperature();
        float humidity = dht.getHumidity();
        lcd.print(temp, 1); // 00.0
        lcd.print("C ");
        lcd.print(humidity, 1); // 000.0
        lcd.print("% ");
    }
    
    selection_cursor();

    delay(300);
}

void write_time(uint16_t time) {
    uint8_t hour = time / 60;
    uint8_t minute = time % 60;

    if (hour < 10) {
        lcd.write('0');
    }
    lcd.print(hour);
    lcd.write(':');
    if (minute < 10) {
        lcd.write('0');
    }
    lcd.print(minute);
}

void redraw_screen() {
    lcd.setCursor(0,0);
    lcd.noBlink();
    lcd.noCursor();
    write_time(now);

    for (uint8_t i = 0; i < NUM_ALARMS; ++i) {
        uint8_t row = 1 + (i / 3);
        uint8_t col = (i % 3) * 6;
        lcd.setCursor(col, row);
        write_time(alarms[i]);
    }
}

void selection_cursor() {
    if (selection != 0) {
        uint8_t row = 1 + ((selection - 1) / 6);
        uint8_t col = ((selection - 1) % 6) * 3;
        lcd.blink();
        lcd.cursor();
        lcd.setCursor(col, row);
    }
}

void ringtone() {
    tone(BUZZER, 125, 10000);
}
