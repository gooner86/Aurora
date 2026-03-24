#include "Mode_AuroraV1.h"
#include "../State.h"

namespace Mode_AuroraV1 {
    void render() {
        static float yDrift = 0;
        yDrift += tempoRate(0.018f, 0.072f) + (bandBassS * 0.04f) + (beatPulse * 0.01f);

        for (int t = 0; t < TUBES; t++) {
            float localBand = tubeBand(t);
            float localWave = tubeBandNeighborhood(t);
            float localLift = tubeBandLevel(t);
            float warp = 6.0f + (localWave * 10.0f) + (localLift * 6.0f);

            for (int y = 0; y < H; y++) {
                uint8_t noise = inoise8(
                    t * 45 + (int)(localBand * 110.0f),
                    (y * 16) - (int)(yDrift * warp),
                    (millis() / 4) + (int)(localLift * 240.0f)
                );

                uint8_t idxP = qadd8(
                    noise,
                    (uint8_t)((localWave * 80.0f) + (localBand * 36.0f) + (beatPulse * 18.0f) + (y * 6))
                );
                CRGB color = ColorFromPalette(palAuroraReal, idxP, 255, LINEARBLEND);

                uint8_t shaped = ease8InOutApprox(noise);
                uint8_t baseGlow = qadd8(58, scale8(shaped, 170));
                color.nscale8_video(baseGlow);

                leds[idx(t, y)] = color;
            }
        }
    }
}
