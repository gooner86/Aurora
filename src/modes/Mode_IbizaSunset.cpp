#include "Mode_IbizaSunset.h"
#include "../State.h"

namespace Mode_IbizaSunset {
    void render() {
        for (int t = 0; t < TUBES; t++) {
            for (int y = 0; y < H; y++) {
                leds[idx(t, y)] = ColorFromPalette(palIbiza, map(y, 0, H-1, 0, 220), 60, LINEARBLEND);
            }
            float sunY = 1.5f + (tubeLevel[t] * (H - 2.5f));
            for (int y = 0; y < H; y++) {
                float dist = fabsf(y - sunY);
                if (dist < 2.5f) {
                    float intensity = powf(1.0f - (dist / 2.5f), 2.0f);
                    CRGB sun = blend(CRGB(255, 140, 0), CRGB(255, 255, 200), (uint8_t)(intensity * 255));
                    leds[idx(t, y)] += sun.nscale8((uint8_t)(intensity * (0.75f + bandBassS * 0.55f) * 255));
                }
            }
        }
    }
}
