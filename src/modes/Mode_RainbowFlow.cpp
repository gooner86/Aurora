#include "Mode_RainbowFlow.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_RainbowFlow::render() {
    uint8_t initialHue = millis() / 20;
    fill_rainbow(leds, NUM_LEDS, initialHue, 7);
}
