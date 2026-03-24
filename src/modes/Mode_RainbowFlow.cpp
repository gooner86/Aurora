#include "Mode_RainbowFlow.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_RainbowFlow::render() {
    static uint16_t startIndex = 0;

    uint8_t speed = (uint8_t)lerpf(1.0f, 15.0f, clamp01(max(volSmooth, tempoReady() ? tempoNormalized : 0.0f)));
    uint8_t brightness = (uint8_t)lerpf(100.0f, 255.0f, clamp01(volSmooth + (beatPulse * 0.15f)));
    startIndex = (uint16_t)(startIndex + speed);

    int logicalIndex = 0;
    for (int t = 0; t < TUBES; t++) {
        for (int y = 0; y < H; y++, logicalIndex++) {
            leds[idx(t, y)] = CHSV((uint8_t)((startIndex / 10) + (logicalIndex * 7)), 255, brightness);
        }
    }
}
