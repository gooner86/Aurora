#include "PhysicalControls.h"
#include "../Hardware.h"
#include "../State.h"

namespace PhysicalControls {
    int lastBrightRaw = -1;
    int lastSensRaw = -1;
    int lastSensValue = -1;
    int brightnessPickupTargetRaw = -1;
    int sensitivityPickupTargetRaw = -1;
    int brightnessNeutralRaw = -1;
    int sensitivityNeutralRaw = -1;
    const int BRIGHTNESS_NOISE_THRESHOLD = 40;
    const int SENSITIVITY_NOISE_THRESHOLD = 20;
    const int PICKUP_WINDOW = 96;
    const int PICKUP_CONFIRM_TICKS = 3;
    const int BRIGHTNESS_FORCE_CAPTURE_DISTANCE = 260;
    const int SENSITIVITY_FORCE_CAPTURE_DISTANCE = 180;
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

    static void applyBrightnessRaw(int raw) {
        uint8_t val = rawToBrightness(raw);
        if (val != USER_BRIGHTNESS || val != globalBrightness) {
            USER_BRIGHTNESS = val;
            globalBrightness = val;
            settingsDirty = true;
        }
    }

    static void applySensitivityRaw(int raw) {
        float newSensitivity = rawToSensitivity(raw);
        int displaySens = (int)lroundf(newSensitivity * 10.0f);
        MASTER_SENSITIVITY = newSensitivity;
        if (displaySens != lastSensValue) {
            lastSensValue = displaySens;
            settingsDirty = true;
        }
    }

    void init() {
        pinMode(PIN_POT_BRIGHTNESS, INPUT);
        pinMode(PIN_POT_SENSITIVITY, INPUT);
        pinMode(PIN_ENC1_SW, INPUT_PULLUP);
        pinMode(PIN_ENC2_SW, INPUT_PULLUP);

        // Start from the real hardware position so soft-takeover logic can
        // detect when the user moves the pot through the current saved value.
        smoothedBrightRaw = (float)analogRead(PIN_POT_BRIGHTNESS);
        smoothedSensRaw = (float)analogRead(PIN_POT_SENSITIVITY);
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
        int targetBrightRaw = (brightnessPickupTargetRaw >= 0) ? brightnessPickupTargetRaw : brightnessToRaw(USER_BRIGHTNESS);
        if (!brightnessCaptured) {
            bool inPickupWindow = abs(smoothBrightInt - targetBrightRaw) <= PICKUP_WINDOW;
            bool movedDeliberately = (brightnessNeutralRaw >= 0) &&
                (abs(smoothBrightInt - brightnessNeutralRaw) >= BRIGHTNESS_FORCE_CAPTURE_DISTANCE);
            if (inPickupWindow) {
                brightnessPickupTicks++;
                if (brightnessPickupTicks >= PICKUP_CONFIRM_TICKS) {
                    brightnessCaptured = true;
                    lastBrightRaw = smoothBrightInt;
                    brightnessPickupTicks = 0;
                }
            } else if (movedDeliberately) {
                brightnessCaptured = true;
                lastBrightRaw = smoothBrightInt;
                brightnessPickupTicks = 0;
                applyBrightnessRaw(smoothBrightInt);
            } else {
                brightnessPickupTicks = 0;
            }
        } else if (abs(smoothBrightInt - lastBrightRaw) > BRIGHTNESS_NOISE_THRESHOLD) {
            lastBrightRaw = smoothBrightInt;
            applyBrightnessRaw(smoothBrightInt);
        }

        int smoothSensInt = (int)lroundf(smoothedSensRaw);
        int targetSensRaw = (sensitivityPickupTargetRaw >= 0) ? sensitivityPickupTargetRaw : sensitivityToRaw(MASTER_SENSITIVITY);
        if (!sensitivityCaptured) {
            bool inPickupWindow = abs(smoothSensInt - targetSensRaw) <= PICKUP_WINDOW;
            bool movedDeliberately = (sensitivityNeutralRaw >= 0) &&
                (abs(smoothSensInt - sensitivityNeutralRaw) >= SENSITIVITY_FORCE_CAPTURE_DISTANCE);
            if (inPickupWindow) {
                sensitivityPickupTicks++;
                if (sensitivityPickupTicks >= PICKUP_CONFIRM_TICKS) {
                    sensitivityCaptured = true;
                    lastSensRaw = smoothSensInt;
                    lastSensValue = (int)lroundf(MASTER_SENSITIVITY * 10.0f);
                    sensitivityPickupTicks = 0;
                }
            } else if (movedDeliberately) {
                sensitivityCaptured = true;
                lastSensRaw = smoothSensInt;
                sensitivityPickupTicks = 0;
                applySensitivityRaw(smoothSensInt);
            } else {
                sensitivityPickupTicks = 0;
            }
        } else if (abs(smoothSensInt - lastSensRaw) > SENSITIVITY_NOISE_THRESHOLD) {
            lastSensRaw = smoothSensInt;
            applySensitivityRaw(smoothSensInt);
        }
    }

    void setBrightnessFromRemote(uint8_t val) {
        brightnessPickupTargetRaw = brightnessToRaw(val);
        brightnessNeutralRaw = (smoothedBrightRaw >= 0.0f) ? (int)lroundf(smoothedBrightRaw) : analogRead(PIN_POT_BRIGHTNESS);
        lastBrightRaw = -1;
        brightnessCaptured = false;
        brightnessPickupTicks = 0;
    }

    void setSensitivityFromRemote(float sens) {
        float s = constrain(sens, 0.05f, 3.0f);
        sensitivityPickupTargetRaw = sensitivityToRaw(s);
        sensitivityNeutralRaw = (smoothedSensRaw >= 0.0f) ? (int)lroundf(smoothedSensRaw) : analogRead(PIN_POT_SENSITIVITY);
        lastSensRaw = -1;
        lastSensValue = (int)lroundf(s * 10.0f);
        sensitivityCaptured = false;
        sensitivityPickupTicks = 0;
    }
}
