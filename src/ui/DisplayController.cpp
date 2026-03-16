#include "DisplayController.h"
#include "../Hardware.h"
#include "../State.h"
#include "Icons.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace DisplayController {

    Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

    static int brightnessPercent(uint8_t value) {
        int clamped = constrain((int)value, 5, 255);
        return (int)lroundf(((float)(clamped - 5) / 250.0f) * 100.0f);
    }

    // State Tracking
    uint8_t lastMode = 255;
    int lastBriPercent = -1;
    int lastSensTenths = -1;
    AudioSource lastAudio = (AudioSource)99;
    PowerState lastPower = (PowerState)99;
    PlayMode lastPlayMode = (PlayMode)99;

    const char* getModeName(uint8_t modeInt) {
        switch(modeInt) {
            case 0: return "AURORA V1";
            case 1: return "EQ SPECTRUM";
            case 2: return "VACUUM TUBE";
            case 3: return "AURORA V2";
            case 4: return "RAINBOW FLOW";
            case 5: return "COLOR VEIL";
            case 6: return "AURORA V3";
            case 7: return "NIGHT RAIN";
            case 8: return "DEEP OCEAN";
            case 9: return "IBIZA SUNSET";
            case 10: return "PSILO WANDER";
            default: return "UNKNOWN";
        }
    }

    const char* getSourceName(AudioSource src) {
        switch(src) {
            case AudioSource::MIC_IN:  return "MIC";
            case AudioSource::LINE_IN: return "LINE";
            case AudioSource::OFF:     return "AMBIENT";
            default: return "";
        }
    }

    void playBootAnimation() {
        // Run a 2-second boot animation
        for (int i = 0; i <= 100; i += 2) {
            display.clearDisplay();

            // 1. Expanding "Sonar" Rings
            display.drawCircle(64, 25, i / 2, SSD1306_WHITE);
            if (i > 30) display.drawCircle(64, 25, (i - 30) / 2, SSD1306_WHITE);
            if (i > 60) display.drawCircle(64, 25, (i - 60) / 2, SSD1306_WHITE);

            // 2. Title Text (Centered)
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(28, 18);
            display.print(F("AURORA"));

            // 3. Loading Bar Frame
            display.drawRoundRect(14, 48, 100, 8, 3, SSD1306_WHITE);
            
            // 4. Loading Bar Fill
            int fillWidth = (i * 96) / 100;
            display.fillRoundRect(16, 50, fillWidth, 4, 2, SSD1306_WHITE);

            // 5. System Status Text
            display.setTextSize(1);
            display.setCursor(44, 58);
            if (i < 30) display.print(F("Booting."));
            else if (i < 60) display.print(F("Booting.."));
            else if (i < 90) display.print(F("Booting..."));
            else display.print(F("Ready!"));

            display.display();
            delay(20); // Controls the speed of the animation
        }
        delay(400); // Pause on the "Ready!" screen for a fraction of a second
    }

    void init() {
        Wire.begin(OLED_SDA, OLED_SCL);
        
        if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
            Serial.println(F("SSD1306 allocation failed"));
            return;
        }

        playBootAnimation();
        
        // Force the UI to draw on the very first loop
        lastMode = 255; 
    }

    void update() {
        int briPercent = brightnessPercent(globalBrightness);
        int sensTenths = (int)lroundf(MASTER_SENSITIVITY * 10.0f);
        float displaySensitivity = sensTenths / 10.0f;

        if (ACTIVE_MODE_INT == lastMode && 
            briPercent == lastBriPercent &&
            sensTenths == lastSensTenths &&
            currentAudio == lastAudio &&
            currentPower == lastPower &&
            currentPlayMode == lastPlayMode) {
            return; 
        }

        lastMode = ACTIVE_MODE_INT;
        lastBriPercent = briPercent;
        lastSensTenths = sensTenths;
        lastAudio = currentAudio;
        lastPower = currentPower;
        lastPlayMode = currentPlayMode;

        display.clearDisplay();

        // --- STANDBY SCREENSAVER ---
        if (currentPower == PowerState::STANDBY) {
            // Draw a minimalist floating "Zzz" or Standby text
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            // Move it around slightly based on time to prevent OLED burn-in
            int yPos = 25 + (sin(millis() / 1000.0) * 5); 
            display.setCursor(20, yPos);
            display.print(F("STANDBY"));
            display.display();
            return;
        }

        // --- TOP ROW: Brightness & Sensitivity ---
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        
        display.drawBitmap(0, 0, Icons::sun_16x16, 16, 16, SSD1306_WHITE);
        display.setCursor(20, 4);
        display.print(briPercent); display.print(F("%"));

        display.drawBitmap(72, 0, Icons::gauge_16x16, 16, 16, SSD1306_WHITE);
        display.setCursor(92, 4);
        display.print(displaySensitivity, 1); display.print(F("x"));

        display.drawLine(0, 18, 128, 18, SSD1306_WHITE);

        // --- MIDDLE ROW: Active Mode (Sleek "Pill" UI) ---
        const char* modeName = getModeName(ACTIVE_MODE_INT);
        int16_t textWidth = strlen(modeName) * 6; 
        
        // Draw the solid white pill background
        int16_t pillX = (128 - (textWidth + 12)) / 2;
        display.fillRoundRect(pillX, 26, textWidth + 12, 16, 4, SSD1306_WHITE);
        
        // Draw the inverted (black) text over it
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        display.setCursor(pillX + 6, 30);
        display.print(modeName);

        // Reset text color for the bottom row
        display.setTextColor(SSD1306_WHITE);

        // --- BOTTOM ROW: Audio Source & AutoPilot ---
        display.setCursor(0, 52);
        display.print(F("IN:"));
        display.print(getSourceName(currentAudio));

        if (currentPlayMode == PlayMode::AUTOPILOT) {
            // Draw an inverted "AUTO" badge on the right
            display.fillRoundRect(98, 50, 30, 12, 2, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(101, 52);
            display.print(F("AUTO"));
        }

        display.display();
    }
}
