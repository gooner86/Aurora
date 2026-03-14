#include "PhysicalControls.h"
#include "../Hardware.h"
#include "../State.h"

namespace PhysicalControls {
    int lastBrightRaw = -1;
    const int ADC_NOISE_THRESHOLD = 40;
    // Smoothing state for ADC to reduce jitter
    static float smoothedBrightRaw = -1.0f;
    static float smoothedSensRaw = -1.0f;

    void init() {
        pinMode(PIN_POT_BRIGHTNESS, INPUT);
        pinMode(PIN_POT_SENSITIVITY, INPUT);
        pinMode(PIN_ENC1_SW, INPUT_PULLUP);
        pinMode(PIN_ENC2_SW, INPUT_PULLUP);
    }

    void tick() {
        int brightRaw = analogRead(PIN_POT_BRIGHTNESS);
        int sensRaw = analogRead(PIN_POT_SENSITIVITY);

        // Initialize smoothing state on first run
        if (smoothedBrightRaw < 0) smoothedBrightRaw = brightRaw;
        if (smoothedSensRaw < 0) smoothedSensRaw = sensRaw;

        // Exponential smoothing
        const float alpha = 0.08f;
        smoothedBrightRaw = (smoothedBrightRaw * (1.0f - alpha)) + (brightRaw * alpha);
        smoothedSensRaw = (smoothedSensRaw * (1.0f - alpha)) + (sensRaw * alpha);

        int smoothBrightInt = (int)smoothedBrightRaw;
        if (abs(smoothBrightInt - lastBrightRaw) > ADC_NOISE_THRESHOLD) {
            lastBrightRaw = smoothBrightInt;
            uint8_t val = map(smoothBrightInt, 0, 4095, 5, 255);
            if (val != USER_BRIGHTNESS) {
                USER_BRIGHTNESS = val;
                globalBrightness = val;
                settingsDirty = true;
            }
        }

        // Update sensitivity but don't save every tiny change; only update runtime
        uint8_t sensVal = map((int)smoothedSensRaw, 0, 4095, 5, 255);
        // Scale mapped pot to sensible sensitivity range (0.05 - 3.0)
        float sensMin = 0.05f;
        float sensMax = 3.0f;
        float sensFloat = sensMin + ((float)(sensVal - 5) / (255 - 5)) * (sensMax - sensMin);
        MASTER_SENSITIVITY = constrain(sensFloat, sensMin, sensMax);
    }
}