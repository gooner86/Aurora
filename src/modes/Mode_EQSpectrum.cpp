#include "Mode_EQSpectrum.h"
#include "../State.h"

namespace Mode_EQSpectrum {
    void render() {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        for (int t = 0; t < TUBES; t++) {
            float surf = tubeLevel[t] * H;
            for (int y = 0; y < H; y++) {
                uint8_t val = barBrightness(surf, y);
                if (val > 0) leds[idx(t, y)] = CHSV(map(y, 0, H-1, 96, 0), 255, val);
            }
            int peakY = (int)(tubePeak[t] * H);
            if (peakY >= 0 && peakY < H) leds[idx(t, peakY)] = CRGB::White;
        }
    }
}
