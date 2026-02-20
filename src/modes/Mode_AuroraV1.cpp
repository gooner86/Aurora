#include "Mode_AuroraV1.h"
#include "../State.h"

namespace Mode_AuroraV1 {
    void render() {
        static float yDrift = 0;
        yDrift += 0.05f;

        for (int t = 0; t < TUBES; t++) {
            for (int y = 0; y < H; y++) {
                // Generate plasma noise
                uint8_t noise = inoise8(t * 45, (y * 15) - (int)(yDrift * 10), millis() / 3);
                
                // Map noise and bass energy to the palette
                uint8_t idxP = qadd8(qsub8(noise, 40), (uint8_t)(bandBassS * 55.0f));
                CRGB color = ColorFromPalette(palAuroraReal, idxP, 255, LINEARBLEND);

                // Apply a glow effect based on noise and volume
                uint8_t shaped = ease8InOutApprox(noise);
                float glow = (0.38f + bandBassS * 0.62f);
                color.nscale8((uint8_t)(shaped * glow * 255));

                leds[idx(t, y)] = color;
            }
        }
    }
}
