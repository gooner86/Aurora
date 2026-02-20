#include "Mode_NightRain.h"
#include "../State.h"

namespace Mode_NightRain {
    void render() {
        static float dropPos[TUBES];
        static float dropSpeed[TUBES];
        static bool tubeActive[TUBES] = {false};

        fadeToBlackBy(leds, NUM_LEDS, 28);

        for (int t = 0; t < TUBES; t++) {
            if (!tubeActive[t] && random8() < (2 + (int)(tubeBandsSmooth[t] * 70))) {
                tubeActive[t] = true;
                dropPos[t] = (float)(H - 1);
                dropSpeed[t] = 0.06f + (bandBassS * 0.18f);
            }
            if (tubeActive[t]) {
                dropPos[t] -= dropSpeed[t];
                int y = (int)dropPos[t];
                if (y >= 0 && y < H) leds[idx(t, y)] += CRGB(80, 100, 130);
                if (dropPos[t] < -1.0f) tubeActive[t] = false;
            }
        }
    }
}
