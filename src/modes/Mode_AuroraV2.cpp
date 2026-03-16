#include "Mode_AuroraV2.h"
#include "../State.h"

namespace Mode_AuroraV2 {
    void render() {
        static float skyPos = 0;
        skyPos += 0.8f + (bandBassS * 2.0f);

        for (int t = 0; t < TUBES; t++) {
            float localBand = tubeBand(t);
            float localBlend = tubeBandNeighborhood(t);
            float sparkleDensity = clamp01((tubeBandFast(t) * 1.7f) + (bandHighS * 0.8f));

            for (int y = 0; y < H; y++) {
                uint8_t baseNoise = inoise8(t * 40, y * 20, (int)skyPos);

                // Background Sky
                CRGB bg;
                if (baseNoise < 128) bg = blend(CRGB(0, 5, 40), CRGB(30, 0, 60), baseNoise * 2);
                else bg = blend(CRGB(30, 0, 60), CRGB(60, 10, 90), (baseNoise - 128) * 2);

                // Borealis Curtain
                uint8_t curtain = inoise8(t * 55, (y * 18) - (int)(skyPos * 0.6f), millis() / 6);
                CRGB aur = ColorFromPalette(palBorealis, curtain, 180, LINEARBLEND);
                nblend(bg, aur, (uint8_t)(70 + (localBlend * 120.0f)));

                // High-freq Sparkles
                uint8_t sparkleNoise = inoise8(t * 90, y * 90, millis());
                uint8_t threshold = 252 - (uint8_t)(sparkleDensity * 180);
                if (sparkleNoise > threshold) {
                    uint8_t intensity = map(sparkleNoise, threshold, 255, 0, 220);
                    CRGB highlight = ((t + y) & 1) ? CRGB(0, 255, 220) : CRGB(255, 20, 110);
                    nblend(bg, highlight, intensity);
                }

                // Local life per tube, with some global ambience under it
                float alive = clamp01(0.18f + (localBand * 0.45f) + (volSmooth * 0.12f));
                bg.nscale8((uint8_t)(255.0f * alive));
                leds[idx(t, y)] = bg;
            }
        }
    }
}
