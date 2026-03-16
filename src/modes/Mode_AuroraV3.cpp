#include "Mode_AuroraV3.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_AuroraV3::render() {
    static float driftA = 0.0f;
    static float driftB = 0.0f;
    driftA += 0.5f + (bandBassS * 1.3f);
    driftB += 0.3f + (bandHighS * 0.9f);

    for (int t = 0; t < TUBES; t++) {
        float localBand = tubeBand(t);
        float localPeak = tubeBandPeak(t);

        for (int y = 0; y < H; y++) {
            uint8_t n1 = inoise8(t * 45, y * 22 - (int)(driftA * 10.0f), millis() / 7);
            uint8_t n2 = inoise8(t * 60 + 1200, y * 30 - (int)(driftB * 12.0f), millis() / 11);
            uint8_t combined = (uint8_t)(((uint16_t)n1 + (uint16_t)n2) / 2);

            uint8_t hue = combined + (uint8_t)(localBand * 40.0f) + (y * 8);
            CRGB color = ColorFromPalette(palAuroraReal, hue, 255, LINEARBLEND);

            float intensity = clamp01(0.16f + (localBand * 0.55f) + (localPeak * 0.25f));
            color.nscale8((uint8_t)(255.0f * intensity));
            leds[idx(t, y)] = color;
        }
    }
}
