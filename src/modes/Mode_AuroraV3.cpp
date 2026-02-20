#include "Mode_AuroraV3.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_AuroraV3::render() {
    uint32_t ms = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(i * 30, ms / 5);
        leds[i] = ColorFromPalette(palAuroraReal, noise + (ms / 10), 255, LINEARBLEND);
    }
}
