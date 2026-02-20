#include "Mode_DeepOcean.h"
#include "../State.h"

namespace Mode_DeepOcean {
    void render() {
        static float wave = 0;
        wave += 0.35f + bandBassS * 1.0f;

        for (int t = 0; t < TUBES; t++) {
            for (int y = 0; y < H; y++) {
                uint8_t caustic = (inoise8(t * 60 + (int)(wave * 20), y * 40, millis() / 10) + inoise8(t * 50, y * 50, millis() / 14)) / 2;
                CRGB c = ColorFromPalette(palDeep, caustic, (uint8_t)(40 + caustic / 3 + bandMidS * 110), LINEARBLEND);
                c.nscale8((uint8_t)(255.0f * lerpf(0.22f, 0.55f, volSmooth)));
                leds[idx(t, y)] = c;
            }
        }
    }
}
