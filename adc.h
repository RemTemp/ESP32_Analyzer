#pragma once
#include "config.h"
#include "flash.h"

struct CaptureSlot {
  uint16_t analog[NUM_CHANNELS][BUFFER_SIZE];
  uint8_t  digital[NUM_DIGITAL][BUFFER_SIZE];
  uint32_t timeUs;
  bool     valid;
  char     label[12];
};

CaptureSlot slots[NUM_SLOTS];
int  activeSlot = 0;
int  viewA = 0;

bool channelEnabled[NUM_CHANNELS] = {true, true, true, true, true, true};
bool digitalEnabled[NUM_DIGITAL]  = {true, true, true, true, true, true, true, true};

int  triggerChannel = 0;
int  triggerLevel   = 2048;
bool triggerRising  = true;

// Длительность окна захвата, мс (0 = максимально быстро)
int  windowMs = 50;

// Калибровка: V = (raw - offset) * scale
float calOffset[NUM_CHANNELS] = {0, 0, 0, 0, 0, 0};
float calScale[NUM_CHANNELS]  = {
  3.3f / 4095, 3.3f / 4095, 3.3f / 4095,
  3.3f / 4095, 3.3f / 4095, 3.3f / 4095
};

uint16_t (*sampleBuffer)[BUFFER_SIZE] = slots[0].analog;

inline float rawToVolt(int ch, uint16_t raw) {
  return (raw - calOffset[ch]) * calScale[ch];
}

inline void initADC() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  for (int i = 0; i < NUM_CHANNELS; i++) pinMode(ADC_PINS[i], INPUT);
  for (int i = 0; i < NUM_DIGITAL; i++)  pinMode(DIG_PINS[i], INPUT_PULLDOWN);
  for (int i = 0; i < 16; i++) analogRead(ADC_PINS[0]);
  for (int s = 0; s < NUM_SLOTS; s++) {
    slots[s].valid = false;
    slots[s].timeUs = 0;
    snprintf(slots[s].label, sizeof(slots[s].label), "Cap%d", s + 1);
  }
  sampleBuffer = slots[0].analog;
  Serial.println("ADC ready");
}

inline uint16_t readChannelStable(int ch) {
  analogRead(ADC_PINS[ch]);               // dummy после mux
  delayMicroseconds(SETTLE_US);
  uint32_t sum = 0;
  for (int a = 0; a < AVG_SAMPLES; a++) sum += analogRead(ADC_PINS[ch]);
  return sum / AVG_SAMPLES;
}

inline void doCaptureWithTrigger() {
  CaptureSlot& sl = slots[activeSlot];
  uint32_t timeout = millis() + 3000;
  bool triggered = false;

  if (triggerChannel < NUM_CHANNELS && channelEnabled[triggerChannel]) {
    uint16_t prev = readChannelStable(triggerChannel);
    while (millis() < timeout) {
      uint16_t now = readChannelStable(triggerChannel);
      if (triggerRising) {
        if (prev < triggerLevel && now >= triggerLevel) { triggered = true; break; }
      } else {
        if (prev > triggerLevel && now <= triggerLevel) { triggered = true; break; }
      }
      prev = now;
      delay(0);
    }
  } else if (triggerChannel >= NUM_CHANNELS) {
    int dch = triggerChannel - NUM_CHANNELS;
    if (dch < NUM_DIGITAL && digitalEnabled[dch]) {
      int prev = digitalRead(DIG_PINS[dch]);
      while (millis() < timeout) {
        int now = digitalRead(DIG_PINS[dch]);
        if (triggerRising  && prev == 0 && now == 1) { triggered = true; break; }
        if (!triggerRising && prev == 1 && now == 0) { triggered = true; break; }
        prev = now;
        delay(0);
      }
    } else triggered = true;
  } else triggered = true;

  // период между точками для заданного окна
  uint32_t periodUs = 0;
  if (windowMs > 0)
    periodUs = (uint32_t)windowMs * 1000UL / BUFFER_SIZE;

  uint32_t t0 = micros();
  for (int i = 0; i < BUFFER_SIZE; i++) {
    uint32_t pointStart = micros();

    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      if (channelEnabled[ch])
        sl.analog[ch][i] = readChannelStable(ch);
      else
        sl.analog[ch][i] = 0;
    }
    for (int d = 0; d < NUM_DIGITAL; d++) {
      if (digitalEnabled[d])
        sl.digital[d][i] = digitalRead(DIG_PINS[d]);
      else
        sl.digital[d][i] = 0;
    }

    if (periodUs > 0) {
      while ((micros() - pointStart) < periodUs) {
        delay(0); // не крутить CPU вхолостую жёстко
      }
    }
  }
  sl.timeUs = micros() - t0;
  sl.valid = true;
  viewA = activeSlot;
  sampleBuffer = slots[viewA].analog;
  activeSlot = (activeSlot + 1) % NUM_SLOTS;
  Serial.printf("Capture %s %u us (window %d ms)\n", sl.label, sl.timeUs, windowMs);
}

inline String saveToFile() {
  if (viewA < 0 || !slots[viewA].valid) return "";

  FileEntry table[MAX_FILES];
  readIndex(table);
  int freeId = findFreeSlot(table);
  if (freeId < 0) return "FULL";

  CaptureSlot& sl = slots[viewA];
  uint16_t seq = nextSeq(table);
  uint32_t dataAddr = fileDataAddr(freeId);

  for (uint32_t a = dataAddr; a < dataAddr + FILE_GAP; a += SECTOR_SIZE)
    sectorErase(a);

  uint8_t hdr[16];
  memset(hdr, 0, 16);
  hdr[0] = 'C'; hdr[1] = 'A'; hdr[2] = 'P'; hdr[3] = '4';
  hdr[4] = sl.timeUs & 0xFF;
  hdr[5] = (sl.timeUs >> 8) & 0xFF;
  hdr[6] = (sl.timeUs >> 16) & 0xFF;
  hdr[7] = (sl.timeUs >> 24) & 0xFF;
  hdr[8] = BUFFER_SIZE & 0xFF;
  hdr[9] = (BUFFER_SIZE >> 8) & 0xFF;
  hdr[10] = NUM_CHANNELS;
  hdr[11] = NUM_DIGITAL;
  pageProgram(dataAddr, hdr, 16);

  uint8_t page[256];
  uint32_t addr = dataAddr + 16;
  uint32_t idx = 0;
  auto flush = [&]() {
    if (idx) { pageProgram(addr, page, idx); addr += idx; idx = 0; }
  };

  for (int ch = 0; ch < NUM_CHANNELS; ch++)
    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
      page[idx++] = sl.analog[ch][i] & 0xFF;
      page[idx++] = (sl.analog[ch][i] >> 8) & 0xFF;
      if (idx >= 256) flush();
    }
  for (int d = 0; d < NUM_DIGITAL; d++)
    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
      page[idx++] = sl.digital[d][i] ? 1 : 0;
      if (idx >= 256) flush();
    }
  flush();

  FileEntry& e = table[freeId];
  memset(&e, 0, sizeof(e));
  e.magic0 = 0xCA; e.magic1 = 0xFE;
  e.used = 1;
  e.id = freeId;
  e.seq = seq;
  e.timeUs = sl.timeUs;
  e.points = BUFFER_SIZE;
  e.nA = NUM_CHANNELS;
  e.nD = NUM_DIGITAL;
  snprintf(e.name, sizeof(e.name), "S%03u", seq);
  writeIndex(table);

  Serial.printf("Saved %s\n", e.name);
  return String(e.name);
}

inline bool deleteFile(uint8_t id) {
  if (id >= MAX_FILES) return false;
  FileEntry table[MAX_FILES];
  readIndex(table);
  if (!(table[id].used && table[id].magic0 == 0xCA)) return false;
  table[id].used = 0;
  table[id].magic0 = 0;
  writeIndex(table);
  return true;
}
