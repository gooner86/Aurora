#include "Mode_VacuumTube.h"
#include "../State.h"
#include "../Hardware.h"
#include <math.h>

void Mode_VacuumTube::render() {
    static float filamentTemp[TUBES] = {0};
    const float THERMAL_ATTACK = 0.04f;
    const float THERMAL_COOLING = 0.004f;

    CRGB colorCold = CRGB(30, 0, 0);
    CRGB colorWarm = CRGB(255, 80, 0);
    CRGB colorHot = CRGB(255, 255, 255);

    fill_solid(leds, NUM_LEDS, CRGB::Black);

    for (int t = 0; t < TUBES; t++) {
        float energy = volSmooth;

        if (energy > filamentTemp[t]) {
            filamentTemp[t] += (energy - filamentTemp[t]) * THERMAL_ATTACK;
        } else {
            filamentTemp[t] -= THERMAL_COOLING;
        }
        filamentTemp[t] = constrain(filamentTemp[t], 0.0f, 1.0f);

        CRGB filamentColor;
        if (filamentTemp[t] < 0.4f) {
            filamentColor = blend(colorCold, colorWarm, (uint8_t)(filamentTemp[t] * 2.5f * 255.0f));
        } else {
            filamentColor = blend(colorWarm, colorHot, (uint8_t)((filamentTemp[t] - 0.4f) * 1.66f * 255.0f));
        }

        float flicker = 1.0f + (sinf(millis() * 0.0022f + t * 0.7f) * 0.04f);
        for (int y = 0; y < H; y++) {
            float dist = fabsf(y - (H / 2.0f)) / (H / 2.0f);
            float glowShape = 1.0f - (dist * 0.7f);

            float brightness = (0.15f + filamentTemp[t] * 0.85f) * flicker;
            CRGB pixel = filamentColor;
            pixel.nscale8((uint8_t)(clamp01(brightness * glowShape) * 255.0f));
            leds[idx(t, y)] = pixel;
        }
    }
}
