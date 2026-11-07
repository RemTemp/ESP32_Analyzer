#pragma once
#include <SPI.h>
#include "config.h"

inline void csLow()  { digitalWrite(CS_PIN, LOW);  delayMicroseconds(1); }
inline void csHigh() { delayMicroseconds(1); digitalWrite(CS_PIN, HIGH); delayMicroseconds(1); }
inline uint8_t xfer(uint8_t b) { return SPI.transfer(b); }

inline uint8_t readStatus1() {
  csLow(); xfer(0x05); uint8_t s = xfer(0); csHigh(); return s;
}
inline void writeEnable() {
  csLow(); xfer(0x06); csHigh(); delayMicroseconds(30);
}
inline bool waitBusy(uint32_t ms = 500) {
  uint32_t t = millis();
  while (millis() - t < ms) {
    if ((readStatus1() & 0x01) == 0) return true;
    delay(1);
  }
  return false;
}
inline void unlockFlash() {
  writeEnable();
  csLow(); xfer(0x01); xfer(0x00); xfer(0x00); csHigh();
  waitBusy(150); delay(20);
}
inline void sectorErase(uint32_t addr) {
  writeEnable();
  csLow();
  xfer(0x20);
  xfer((addr >> 16) & 0xFF);
  xfer((addr >>  8) & 0xFF);
  xfer( addr        & 0xFF);
  csHigh();
  waitBusy(400);
}
inline void pageProgram(uint32_t addr, const uint8_t* data, uint32_t len) {
  while (len) {
    uint32_t off = addr & 0xFF;
    uint32_t chunk = min(len, 256UL - off);
    writeEnable();
    csLow();
    xfer(0x02);
    xfer((addr >> 16) & 0xFF);
    xfer((addr >>  8) & 0xFF);
    xfer( addr        & 0xFF);
    for (uint32_t i = 0; i < chunk; i++) xfer(data[i]);
    csHigh();
    waitBusy(80);
    addr += chunk; data += chunk; len -= chunk;
  }
}
inline void flashRead(uint32_t addr, uint8_t* buf, uint32_t len) {
  csLow();
  xfer(0x03);
  xfer((addr >> 16) & 0xFF);
  xfer((addr >>  8) & 0xFF);
  xfer( addr        & 0xFF);
  for (uint32_t i = 0; i < len; i++) buf[i] = xfer(0);
  csHigh();
}

inline bool initFlash() {
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  SPI.begin(18, 19, 23, -1);
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  csLow(); xfer(0x9F);
  uint8_t id0 = xfer(0), id1 = xfer(0), id2 = xfer(0);
  csHigh();
  uint32_t jedec = ((uint32_t)id0 << 16) | ((uint32_t)id1 << 8) | id2;
  Serial.printf("JEDEC: 0x%06X\n", jedec);
  if (jedec != 0xEF4016) return false;
  unlockFlash();
  return true;
}

// ===== Простая FS =====
// Запись индекса: 24 слота × 32 байта = 768 байт в IDX_ADDR
// entry: magic(2)=0xCA 0xFE, used(1), id(1), seq(2), timeUs(4), points(2), nA(1), nD(1), reserved(18)

struct FileEntry {
  uint8_t  magic0, magic1;
  uint8_t  used;
  uint8_t  id;
  uint16_t seq;
  uint32_t timeUs;
  uint16_t points;
  uint8_t  nA, nD;
  char     name[12];
  uint8_t  reserved[6];
};  // 32 bytes


inline uint32_t fileDataAddr(uint8_t id) {
  return DATA_ADDR + (uint32_t)id * FILE_GAP;
}

inline void readIndex(FileEntry* table) {
  flashRead(IDX_ADDR, (uint8_t*)table, sizeof(FileEntry) * MAX_FILES);
}

inline void writeIndex(const FileEntry* table) {
  sectorErase(IDX_ADDR);
  pageProgram(IDX_ADDR, (const uint8_t*)table, sizeof(FileEntry) * MAX_FILES);
}

inline int countUsedFiles(const FileEntry* table) {
  int n = 0;
  for (int i = 0; i < MAX_FILES; i++)
    if (table[i].used && table[i].magic0 == 0xCA && table[i].magic1 == 0xFE) n++;
  return n;
}

inline int findFreeSlot(const FileEntry* table) {
  for (int i = 0; i < MAX_FILES; i++)
    if (!(table[i].used && table[i].magic0 == 0xCA)) return i;
  return -1;
}

inline uint16_t nextSeq(const FileEntry* table) {
  uint16_t m = 0;
  for (int i = 0; i < MAX_FILES; i++)
    if (table[i].used && table[i].seq > m) m = table[i].seq;
  return m + 1;
}

// Размер одного файла данных (байт)
inline uint32_t captureDataSize(uint16_t points, uint8_t nA, uint8_t nD) {
  return (uint32_t)points * nA * 2 + (uint32_t)points * nD;
}
