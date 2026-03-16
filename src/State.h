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

extern uint32_t sleepStartTime;
extern uint32_t sleepDurationMs;

// LED Buffers
extern CRGB leds[NUM_LEDS];
extern CRGB lastFrame[NUM_LEDS];

// Audio & Physics State
extern uint8_t bandBass8;
extern float tubeLevel[TUBES];
extern float tubePeak[TUBES];
extern float tubeBandsSmooth[TUBES];
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