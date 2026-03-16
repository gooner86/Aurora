#include "PhysicalControls.h"
#include "../Hardware.h"
#include "../State.h"

namespace PhysicalControls {
    int lastBrightRaw = -1;
    int lastSensRaw = -1;
    int lastSensValue = -1;
    const int BRIGHTNESS_NOISE_THRESHOLD = 40;
    const int SENSITIVITY_NOISE_THRESHOLD = 20;
    const int PICKUP_WINDOW = 96;
    const int PICKUP_CONFIRM_TICKS = 3;
    // Smoothing state for ADC to reduce jitter
    static float smoothedBrightRaw = -1.0f;
    static float smoothedSensRaw = -1.0f;
    static bool brightnessCaptured = false;
    static bool sensitivityCaptured = false;
    static int brightnessPickupTicks = 0;
    static int sensitivityPickupTicks = 0;

    static int brightnessToRaw(uint8_t val) {
        return map(constrain((int)val, 5, 255), 5, 255, 0, 4095);
    }

    static uint8_t rawToBrightness(int raw) {
        return (uint8_t)constrain(map(constrain(raw, 0, 4095), 0, 4095, 5, 255), 5, 255);
    }

    static float rawToSensitivity(int raw) {
        float sensMin = 0.05f;
        float sensMax = 3.0f;
        uint8_t sensVal = rawToBrightness(raw);
        float sensFloat = sensMin + ((float)(sensVal - 5) / (255 - 5)) * (sensMax - sensMin);
        return constrain(sensFloat, sensMin, sensMax);
    }

    static int sensitivityToRaw(float sens) {
        float sensMin = 0.05f;
        float sensMax = 3.0f;
        float s = constrain(sens, sensMin, sensMax);
        float scaled = 5.0f + ((s - sensMin) / (sensMax - sensMin)) * (255.0f - 5.0f);
        float ratio = (scaled - 5.0f) / (255.0f - 5.0f);
        return (int)lroundf(ratio * 4095.0f);
    }

    void init() {
        pinMode(PIN_POT_BRIGHTNESS, INPUT);
        pinMode(PIN_POT_SENSITIVITY, INPUT);
        pinMode(PIN_ENC1_SW, INPUT_PULLUP);
        pinMode(PIN_ENC2_SW, INPUT_PULLUP);

        // Seed the analog smoothing with the current runtime settings so
        // the pots do not override BLE/saved values until they are moved into range.
        setBrightnessFromRemote(USER_BRIGHTNESS);
        setSensitivityFromRemote(MASTER_SENSITIVITY);
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

        int smoothBrightInt = (int)lroundf(smoothedBrightRaw);
        int targetBrightRaw = brightnessToRaw(USER_BRIGHTNESS);
        if (!brightnessCaptured) {
            if (abs(smoothBrightInt - targetBrightRaw) <= PICKUP_WINDOW) {
                brightnessPickupTicks++;
                if (brightnessPickupTicks >= PICKUP_CONFIRM_TICKS) {
                    brightnessCaptured = true;
                    lastBrightRaw = smoothBrightInt;
                    brightnessPickupTicks = 0;
                }
            } else {
                brightnessPickupTicks = 0;
            }
        } else if (abs(smoothBrightInt - lastBrightRaw) > BRIGHTNESS_NOISE_THRESHOLD) {
            lastBrightRaw = smoothBrightInt;
            uint8_t val = rawToBrightness(smoothBrightInt);
            if (val != USER_BRIGHTNESS || val != globalBrightness) {
                USER_BRIGHTNESS = val;
                globalBrightness = val;
                settingsDirty = true;
            }
        }

        int smoothSensInt = (int)lroundf(smoothedSensRaw);
        int targetSensRaw = sensitivityToRaw(MASTER_SENSITIVITY);
        if (!sensitivityCaptured) {
            if (abs(smoothSensInt - targetSensRaw) <= PICKUP_WINDOW) {
                sensitivityPickupTicks++;
                if (sensitivityPickupTicks >= PICKUP_CONFIRM_TICKS) {
                    sensitivityCaptured = true;
                    lastSensRaw = smoothSensInt;
                    lastSensValue = (int)lroundf(MASTER_SENSITIVITY * 10.0f);
                    sensitivityPickupTicks = 0;
                }
            } else {
                sensitivityPickupTicks = 0;
            }
        } else if (abs(smoothSensInt - lastSensRaw) > SENSITIVITY_NOISE_THRESHOLD) {
            lastSensRaw = smoothSensInt;
            float newSensitivity = rawToSensitivity(smoothSensInt);
            int displaySens = (int)lroundf(newSensitivity * 10.0f);
            MASTER_SENSITIVITY = newSensitivity;
            if (displaySens != lastSensValue) {
                lastSensValue = displaySens;
                settingsDirty = true;
            }
        }
    }

    void setBrightnessFromRemote(uint8_t val) {
        int raw = brightnessToRaw(val);
        smoothedBrightRaw = (float)raw;
        lastBrightRaw = raw;
        brightnessCaptured = false;
        brightnessPickupTicks = 0;
    }

    void setSensitivityFromRemote(float sens) {
        float s = constrain(sens, 0.05f, 3.0f);
        int raw = sensitivityToRaw(s);
        smoothedSensRaw = (float)raw;
        lastSensRaw = raw;
        lastSensValue = (int)lroundf(s * 10.0f);
        sensitivityCaptured = false;
        sensitivityPickupTicks = 0;
    }
}
