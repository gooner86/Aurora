#include "Mode_EQSpectrum.h"
#include "../State.h"

namespace Mode_EQSpectrum {
    void render() {
        static float displayLevel[TUBES] = {0};
        static float peakLevel[TUBES] = {0};
        static uint32_t peakHoldTime[TUBES] = {0};
        uint32_t now = millis();
        float sensNorm = clamp01((MASTER_SENSITIVITY - 0.05f) / (3.0f - 0.05f));
        float deadZone = lerpf(0.14f, 0.03f, sensNorm);
        float gain = lerpf(0.95f, 1.25f, sensNorm);
        float responseCurve = lerpf(1.85f, 1.20f, sensNorm);

        fill_solid(leds, NUM_LEDS, CRGB::Black);
        for (int t = 0; t < TUBES; t++) {
            float fast = tubeBandFast(t);
            float smooth = tubeBand(t);
            float rawTarget = max(smooth * 0.90f, fast * 1.10f);
            float target = clamp01((rawTarget - deadZone) * gain);
            target = powf(target, responseCurve);

            float attack = 0.72f;
            float release = 0.30f;
            float k = (target > displayLevel[t]) ? attack : release;
            displayLevel[t] += (target - displayLevel[t]) * k;

            if (displayLevel[t] > peakLevel[t]) {
                peakLevel[t] = displayLevel[t];
                peakHoldTime[t] = now;
            } else if ((now - peakHoldTime[t]) > 140) {
                peakLevel[t] = max(displayLevel[t], peakLevel[t] - 0.075f);
            }

            float surf = displayLevel[t] * H;
            for (int y = 0; y < H; y++) {
                uint8_t val = barBrightness(surf, y);
                if (val > 0) leds[idx(t, y)] = CHSV(map(y, 0, H-1, 96, 0), 255, val);
            }
            int peakY = (int)(peakLevel[t] * H);
            if (peakY >= 0 && peakY < H) leds[idx(t, peakY)] = CRGB::White;
        }
    }
}
