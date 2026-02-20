#include "Mode_SolidColor.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_SolidColor::render() {
    // Uses the global 'hue' variable from State.h which is tied to your pots
    fill_solid(leds, NUM_LEDS, CHSV(globalHue, 255, 255));
}