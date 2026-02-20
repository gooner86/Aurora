#include "PhysicalControls.h"
#include "../Hardware.h"
#include "../State.h"

namespace PhysicalControls {
    int lastBrightRaw = -1;
    const int ADC_NOISE_THRESHOLD = 40;

    void init() {
        pinMode(PIN_POT_BRIGHTNESS, INPUT);
        pinMode(PIN_POT_SENSITIVITY, INPUT);
        pinMode(PIN_ENC1_SW, INPUT_PULLUP);
        pinMode(PIN_ENC2_SW, INPUT_PULLUP);
    }

    void tick() {
        int brightRaw = analogRead(PIN_POT_BRIGHTNESS);
        if (abs(brightRaw - lastBrightRaw) > ADC_NOISE_THRESHOLD) {
            lastBrightRaw = brightRaw;
            uint8_t val = map(brightRaw, 0, 4095, 5, 255);
            USER_BRIGHTNESS = val;
            globalBrightness = val; 
        }
    }
}