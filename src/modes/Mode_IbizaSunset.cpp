#include "Mode_IbizaSunset.h"
#include "../State.h"

namespace Mode_IbizaSunset {
    void render() {
        for (int t = 0; t < TUBES; t++) {
            float localBand = tubeBand(t);
            float sunY = 1.5f + (tubeLevel[t] * (H - 2.5f));
            float sunRadius = 1.9f + (localBand * 1.2f) + (beatPulse * 0.25f);

            for (int y = 0; y < H; y++) {
                uint8_t baseBri = (uint8_t)(58 + (y * 12));
                leds[idx(t, y)] = ColorFromPalette(palIbiza, map(y, 0, H-1, 0, 220), baseBri, LINEARBLEND);

                float dist = fabsf(y - sunY);
                if (dist < sunRadius) {
                    float intensity = powf(1.0f - (dist / sunRadius), 2.0f);
                    CRGB sun = blend(CRGB(255, 140, 0), CRGB(255, 255, 200), (uint8_t)(intensity * 255));
                    sun.nscale8_video((uint8_t)(intensity * 255.0f));
                    leds[idx(t, y)] += sun;
                }
            }
        }
    }
}
