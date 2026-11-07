#pragma once

#define AP_SSID       "ESP32-Analyzer"
#define AP_PASSWORD   "12345678"

#define CS_PIN        5

#define IDX_ADDR      0x010000UL
#define DATA_ADDR     0x011000UL
#define SECTOR_SIZE   4096UL
#define MAX_FILES     24
#define FILE_GAP      0x3000UL

#define NUM_CHANNELS  6
#define BUFFER_SIZE   512
#define AVG_SAMPLES   2          // меньше = меньше нагрев
#define SETTLE_US     8

const int ADC_PINS[NUM_CHANNELS] = {36, 39, 34, 35, 32, 33};

#define NUM_DIGITAL   8
const int DIG_PINS[NUM_DIGITAL] = {13, 14, 15, 16, 17, 21, 22, 25};

#define NUM_SLOTS     3
