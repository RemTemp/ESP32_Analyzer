#pragma once
#include <math.h>
#include "config.h"

// Простой radix-2 FFT (in-place), N должен быть степенью 2
// Работает с float-массивами re[] и im[]

#define FFT_SIZE  256   // берём 256 точек из буфера (степень 2)

float fftRe[FFT_SIZE];
float fftIm[FFT_SIZE];
float fftMag[FFT_SIZE / 2];   // амплитуды (только положительные частоты)
float sampleRateHz = 0;       // Гц, заполняется после захвата

inline void fftBitReverse(float* re, float* im, int n) {
  int j = 0;
  for (int i = 0; i < n - 1; i++) {
    if (i < j) {
      float tr = re[i]; re[i] = re[j]; re[j] = tr;
      float ti = im[i]; im[i] = im[j]; im[j] = ti;
    }
    int k = n >> 1;
    while (k <= j) { j -= k; k >>= 1; }
    j += k;
  }
}

inline void computeFFT(float* re, float* im, int n) {
  fftBitReverse(re, im, n);

  for (int len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * PI / len;
    float wlenRe = cosf(ang);
    float wlenIm = sinf(ang);

    for (int i = 0; i < n; i += len) {
      float wRe = 1.0f, wIm = 0.0f;
      for (int j = 0; j < len / 2; j++) {
        float uRe = re[i + j];
        float uIm = im[i + j];
        float vRe = re[i + j + len/2] * wRe - im[i + j + len/2] * wIm;
        float vIm = re[i + j + len/2] * wIm + im[i + j + len/2] * wRe;
        re[i + j] = uRe + vRe;
        im[i + j] = uIm + vIm;
        re[i + j + len/2] = uRe - vRe;
        im[i + j + len/2] = uIm - vIm;
        float nextWRe = wRe * wlenRe - wIm * wlenIm;
        wIm = wRe * wlenIm + wIm * wlenRe;
        wRe = nextWRe;
      }
    }
  }
}

// Считает спектр по каналу ch (данные уже в sampleBuffer)
// sampleRateHz нужно передать (или посчитать из captureTimeUs)
inline void runFFT(int ch, float rateHz) {
  sampleRateHz = rateHz;

  // Берём первые FFT_SIZE точек, убираем постоянную составляющую (DC)
  float mean = 0;
  for (int i = 0; i < FFT_SIZE; i++) {
    mean += sampleBuffer[ch][i];
  }
  mean /= FFT_SIZE;

  // Окно Ханна + заполнение
  for (int i = 0; i < FFT_SIZE; i++) {
    float window = 0.5f * (1.0f - cosf(2.0f * PI * i / (FFT_SIZE - 1)));
    fftRe[i] = (sampleBuffer[ch][i] - mean) * window;
    fftIm[i] = 0;
  }

  computeFFT(fftRe, fftIm, FFT_SIZE);

  // Амплитуды (нормировка)
  for (int i = 0; i < FFT_SIZE / 2; i++) {
    float mag = sqrtf(fftRe[i]*fftRe[i] + fftIm[i]*fftIm[i]) * 2.0f / FFT_SIZE;
    fftMag[i] = mag;
  }
}

// Частота бина i
inline float binFreq(int i) {
  return i * sampleRateHz / FFT_SIZE;
}
