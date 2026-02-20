#include <Arduino.h>
#include <FastLED.h>

// 1. Project Map & Hardware Config
#include "State.h"
#include "Hardware.h"

// 2. Subsystem Headers
#include "audio/MicInput.h"
#include "audio/LineInput.h"
#include "core/AudioAnalyzer.h"
#include "core/VisualPhysics.h"
#include "core/Transition.h"
#include "core/ModeManager.h"
#include "input/BLEController.h"
#include "input/PhysicalControls.h"
#include "sys/SettingsManager.h"
#include "sys/OTAUpdater.h"
#include "sys/AutoPilot.h"
#include "ui/DisplayController.h"

// ==========================================
// 1. INSTANTIATE GLOBAL STATE
// ==========================================
PowerState  currentPower    = PowerState::AWAKE;
AudioSource currentAudio    = AudioSource::MIC_IN;
PlayMode    currentPlayMode = PlayMode::MANUAL;

int ACTIVE_MODE_INT         = 0; // Starts at 0 (Aurora) to guarantee light on boot
uint8_t USER_BRIGHTNESS     = 90;
float MASTER_SENSITIVITY    = 1.0f;
uint8_t globalHue           = 0;
uint8_t globalBrightness    = 200;

uint32_t sleepStartTime     = 0;
uint32_t sleepDurationMs    = 0;

CRGB leds[NUM_LEDS];
CRGB lastFrame[NUM_LEDS];

uint8_t bandBass8           = 0;
float tubeLevel[TUBES]       = {0};
float tubePeak[TUBES]        = {0};
float tubeBandsSmooth[TUBES] = {0};
float bandBassS = 0.0f, bandMidS = 0.0f, bandHighS = 0.0f, volSmooth = 0.0f, levelSlow = 0.0f;
bool beatDetected = false;

// ==========================================
// 2. PALETTE INSTANTIATION
// ==========================================
CRGB SOLID_COLOR_VAL = CRGB(0, 255, 204);
uint8_t SOLID_STYLE = 0;

DEFINE_GRADIENT_PALETTE( aurora_gp ) { 0,0,0,0, 40,0,50,20, 100,0,255,50, 160,0,200,200, 210,100,0,220, 255,220,220,255 };
DEFINE_GRADIENT_PALETTE( borealis_gp ) { 0,15,0,30, 70,60,0,120, 140,0,90,255, 200,0,255,140, 255,255,80,190 };
DEFINE_GRADIENT_PALETTE( deep_gp ) { 0,0,0,10, 40,5,0,20, 90,0,5,30, 150,20,0,40, 200,0,20,50, 255,30,20,50 };
DEFINE_GRADIENT_PALETTE( ibiza_gp ) { 0,0,5,20, 60,40,0,30, 100,160,20,0, 140,255,80,0, 200,100,0,80, 255,10,0,40 };

CRGBPalette16 palAuroraReal = aurora_gp;
CRGBPalette16 palBorealis   = borealis_gp;
CRGBPalette16 palDeep       = deep_gp;
CRGBPalette16 palIbiza      = ibiza_gp;

// ==========================================
// 3. SYSTEM SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000); 

    Serial.println("\n--- AURORA V32 BOOT ---");

    // 1. LED INITIALIZATION
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 2000); 
    FastLED.setBrightness(200);
    
    // Test: Orange Flash (You will finally see this!)
    fill_solid(leds, NUM_LEDS, CRGB::Orange);
    FastLED.show();
    delay(2000); 

    // 2. SUBSYSTEMS
    SettingsManager::init();
    ACTIVE_MODE_INT = 0; // Force Aurora V1 for setup

    DisplayController::init(); 
    MicInput::init();    
    LineInput::init();   
    ModeManager::init();
    PhysicalControls::init();
    BLEController::init();

    Serial.println("Boot Sequence Finished.");
    FastLED.clear(true);
    FastLED.show();
}

void loop() {
    OTAUpdater::tick();

    AudioAnalyzer::update();  
    
    // DEBUG: Print Volume Levels to Serial
    static uint32_t lastVoiceTick = 0;
    if (millis() - lastVoiceTick > 500) {
        Serial.print("Mic Volume (volSmooth): ");
        Serial.println(volSmooth); 
        lastVoiceTick = millis();
    }
    
    // Throttled UI/Input to save CPU for the FFT
    static uint32_t lastSlowTick = 0;
    if (millis() - lastSlowTick > 40) {
        PhysicalControls::tick(); 
        DisplayController::update();
        lastSlowTick = millis();
    }

    AudioAnalyzer::update();  
    VisualPhysics::update();  // Safe to use now because we fixed the math crash earlier!
    ModeManager::render();    
    
    // Override pot if it's turned all the way down during testing
    uint8_t outputBri = (globalBrightness < 10) ? 100 : globalBrightness;
    FastLED.setBrightness(outputBri);
    
    FastLED.show();
    yield(); // Vital for ESP32 background tasks
}