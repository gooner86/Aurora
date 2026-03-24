#include "Mode_AuroraV3.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_AuroraV3::render() {
    static float driftA = 0.0f;
    static float driftB = 0.0f;
    driftA += tempoRate(0.22f, 0.86f) + (bandBassS * 0.42f) + (beatPulse * 0.05f);
    driftB += tempoRate(0.12f, 0.48f) + (bandHighS * 0.34f);

    for (int t = 0; t < TUBES; t++) {
        float localBand = tubeBand(t);
        float localPeak = tubeBandPeak(t);
        float localFast = tubeBandFast(t);
        float spikeDrive = clamp01((localPeak * 0.58f) + (localFast * 0.42f));

        for (int y = 0; y < H; y++) {
            uint8_t n1 = inoise8(
                t * 45 + (int)(localBand * 130.0f),
                y * 22 - (int)(driftA * (8.0f + localBand * 10.0f)),
                millis() / 7
            );
            uint8_t n2 = inoise8(
                t * 60 + 1200 + (int)(spikeDrive * 120.0f),
                y * 30 - (int)(driftB * (10.0f + localPeak * 14.0f)),
                millis() / 11
            );
            uint8_t combined = (uint8_t)(((uint16_t)n1 + (uint16_t)n2) / 2);

            uint8_t hue = combined + (uint8_t)(localBand * 40.0f) + (y * 8);
            CRGB color = ColorFromPalette(palAuroraReal, hue, 255, LINEARBLEND);

            uint8_t contour = qadd8(70, scale8(ease8InOutApprox(combined), 145));
            color.nscale8_video(contour);
            leds[idx(t, y)] = color;
        }
    }
}
