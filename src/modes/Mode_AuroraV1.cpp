#include "Mode_AuroraV1.h"
#include "../State.h"

namespace Mode_AuroraV1 {
    void render() {
        static float yDrift = 0;
        yDrift += 0.04f + (bandBassS * 0.10f);

        for (int t = 0; t < TUBES; t++) {
            float localBand = tubeBand(t);
            float localWave = tubeBandNeighborhood(t);
            float localLift = tubeBandLevel(t);

            for (int y = 0; y < H; y++) {
                // Generate plasma noise
                uint8_t noise = inoise8(
                    t * 45 + (int)(localBand * 80.0f),
                    (y * 15) - (int)(yDrift * (8.0f + localWave * 8.0f)),
                    millis() / 3
                );
                
                // Map noise and per-tube energy to the palette
                uint8_t idxP = qadd8(qsub8(noise, 40), (uint8_t)((localWave * 70.0f) + (bandBassS * 25.0f)));
                CRGB color = ColorFromPalette(palAuroraReal, idxP, 255, LINEARBLEND);

                // Give each tube its own glow amount instead of one shared bass pulse
                uint8_t shaped = ease8InOutApprox(noise);
                float glow = clamp01(0.20f + (localLift * 0.55f) + (localBand * 0.25f) + (bandBassS * 0.18f));
                color.nscale8((uint8_t)(shaped * glow * 255));

                leds[idx(t, y)] = color;
            }
        }
    }
}
