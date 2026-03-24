#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include "Hardware.h"

// =====================
// SYSTEM ENUMS
// =====================
enum class PowerState  { AWAKE, STANDBY };
enum class AudioSource { MIC_IN, LINE_IN, OFF }; 
enum class PlayMode    { MANUAL, AUTOPILOT };

// =====================
// GLOBAL STATE (Externs)
// =====================
extern PowerState  currentPower;
extern AudioSource currentAudio;
extern PlayMode    currentPlayMode;

extern int ACTIVE_MODE_INT;          
extern uint8_t globalHue;            
extern uint8_t globalBrightness;     
extern uint8_t USER_BRIGHTNESS;      
extern float MASTER_SENSITIVITY;     

extern CRGB SOLID_COLOR_VAL;
extern uint8_t SOLID_STYLE;
extern uint8_t PSILO_WANDER;
extern uint8_t PSILO_BLOOM;
extern uint8_t PSILO_LUCIDITY;

extern uint32_t sleepStartTime;
extern uint32_t sleepDurationMs;

// LED Buffers
extern CRGB leds[NUM_LEDS];
extern CRGB lastFrame[NUM_LEDS];

// Audio & Physics State
extern uint8_t bandBass8;
extern float tubeBandsInstant[TUBES];
extern float tubeBandsSplitInstant[TUBES];
extern float tubeLevel[TUBES];
extern float tubePeak[TUBES];
extern float tubeBandsSmooth[TUBES];
extern float tubeBandsSplitSmooth[TUBES];
extern float tubeMax[TUBES];

extern float bandBassS;
extern float bandMidS;
extern float bandHighS;
extern float volSmooth;
extern float volBeat;
extern float levelSlow;
extern float levelFast;
extern float bassEnergy;
extern float midEnergy;
extern float highEnergy;
extern float beatThreshold;
extern bool beatDetected;
extern uint32_t lastBeatTime;
extern float beatPulse;
extern float tempoBPM;
extern float tempoConfidence;
extern float tempoNormalized;
extern uint32_t beatIntervalMs;

// Settings dirty flag for debounced NVS writes
extern bool settingsDirty;

// =====================
// PALETTES
// =====================
extern CRGBPalette16 palAuroraReal;
extern CRGBPalette16 palBorealis;
extern CRGBPalette16 palDeep;
extern CRGBPalette16 palIbiza;

// =====================
// INLINE MATH/UI HELPERS
// =====================
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clamp01(float x) { return (x < 0) ? 0 : (x > 1 ? 1 : x); }
inline int uiPercent(float value, float minValue, float maxValue) {
  if (maxValue <= minValue) return 0;
  float normalized = clamp01((value - minValue) / (maxValue - minValue));
  return (int)lroundf(normalized * 100.0f);
}
inline int brightnessPercent(uint8_t value) {
  return uiPercent((float)constrain((int)value, 5, 255), 5.0f, 255.0f);
}
inline int sensitivityPercent(float value) {
  return uiPercent(constrain(value, 0.05f, 3.0f), 0.05f, 3.0f);
}
inline bool tempoReady() { return tempoConfidence > 0.18f && beatIntervalMs >= 240 && beatIntervalMs <= 1500; }
inline float tempoRate(float slow, float fast) { return lerpf(slow, fast, tempoReady() ? clamp01(tempoNormalized) : 0.0f); }

inline int clampTubeIndex(int t) {
  if (t < 0) return 0;
  if (t >= TUBES) return TUBES - 1;
  return t;
}

inline float tubeBand(int t) {
  return tubeBandsSmooth[clampTubeIndex(t)];
}

inline float tubeBandFast(int t) {
  return tubeBandsInstant[clampTubeIndex(t)];
}

inline float tubeBandSplit(int t) {
  return tubeBandsSplitSmooth[clampTubeIndex(t)];
}

inline float tubeBandSplitFast(int t) {
  return tubeBandsSplitInstant[clampTubeIndex(t)];
}

inline float tubeBandLevel(int t) {
  return tubeLevel[clampTubeIndex(t)];
}

inline float tubeBandPeak(int t) {
  return tubePeak[clampTubeIndex(t)];
}

inline float tubeBandNeighborhood(int t) {
  int i = clampTubeIndex(t);
  float sum = tubeBand(i);
  int count = 1;
  if (i > 0) {
    sum += tubeBand(i - 1);
    count++;
  }
  if (i + 1 < TUBES) {
    sum += tubeBand(i + 1);
    count++;
  }
  return sum / (float)count;
}

inline int idx(int t, int y) {
  if (!SERPENTINE) return t * H + y;
  bool rev = (t % 2 == 1);
  return t * H + (rev ? (H - 1 - y) : y);
}

inline uint8_t barBrightness(float surf, int y) {
  float d = surf - y;
  if (d >= 1.0f) return 255;
  if (d >= 0.0f) return (uint8_t)(255 * d);
  return 0;
}
