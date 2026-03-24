#include "Mode_TempoTest.h"
#include "../State.h"

namespace Mode_TempoTest {
    void render() {
        static float hueOffset = 0.0f;
        hueOffset += 0.7f;

        fill_solid(leds, NUM_LEDS, CRGB::Black);
        for (int t = 0; t < TUBES; t++) {
            float surf = tubeLevel[t] * 9.5f;
            for (int y = 0; y < H; y++) {
                uint8_t val = barBrightness(surf, y);
                if (val > 0) {
                    leds[idx(t, y)] = CHSV((uint8_t)(hueOffset + (t * 12) + (y * 8)), 240, val);
                }
            }
        }
    }
}
