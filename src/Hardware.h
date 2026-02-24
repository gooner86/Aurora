#pragma once
#include <Arduino.h>

#define SAFETY_CAP 0.38f  

// =====================
// LED MATRIX
// =====================
#define LED_PIN     13  // Keep on 13 since we know it works now
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

const int TUBES = 11;
const int H     = 9;
const int NUM_LEDS = TUBES * H;
const bool SERPENTINE = true;

// =====================
// AUDIO INPUT 1 (MIC) - I2S_0
// =====================
#define MIC_BCLK    26
#define MIC_WS      25
#define MIC_SD      32 

// =====================
// AUDIO INPUT 2 (LINE-IN PCM1808) - I2S_1
// =====================
#define LINE_SCK    0   
#define LINE_BCLK   4
#define LINE_WS     19
#define LINE_SD     23

// =====================
// OLED DISPLAY (I2C)
// =====================
#define OLED_SDA    21
#define OLED_SCL    22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR   0x3C

// =====================
// PHYSICAL INPUTS
// =====================
#define PIN_POT_BRIGHTNESS  34 
#define PIN_POT_SENSITIVITY 35 

// Rotary Encoder 1: MODE (MOVED CLK away from LED Pin 13)
#define PIN_ENC1_CLK 14 // MOVED to 14
#define PIN_ENC1_DT  12 // MOVED to 12
#define PIN_ENC1_SW  27  

// Rotary Encoder 2: INPUT SOURCE
#define PIN_ENC2_CLK 16
#define PIN_ENC2_DT  17
#define PIN_ENC2_SW  18