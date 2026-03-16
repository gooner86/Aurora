#include "Mode_VacuumTube.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_VacuumTube::render() {
    static float filamentTemp[TUBES] = {0};

    CRGB colorCold = CRGB(30, 0, 0);
    CRGB colorWarm = CRGB(255, 80, 0);
    CRGB colorHot = CRGB(255, 255, 220);

    fill_solid(leds, NUM_LEDS, CRGB::Black);

    for (int t = 0; t < TUBES; t++) {
        float energy = tubeBandNeighborhood(t);
        if (energy > filamentTemp[t]) filamentTemp[t] += (energy - filamentTemp[t]) * 0.05f;
        else filamentTemp[t] *= 0.992f;

        filamentTemp[t] = constrain(filamentTemp[t], 0.0f, 1.0f);

        CRGB filamentColor;
        if (filamentTemp[t] < 0.45f) {
            filamentColor = blend(colorCold, colorWarm, (uint8_t)(filamentTemp[t] / 0.45f * 255.0f));
        } else {
            filamentColor = blend(colorWarm, colorHot, (uint8_t)(((filamentTemp[t] - 0.45f) / 0.55f) * 255.0f));
        }

        for (int y = 0; y < H; y++) {
            float dist = fabsf(y - (H / 2.0f)) / (H / 2.0f);
            float glowShape = 1.0f - (dist * 0.7f);
            uint8_t flicker = qsub8(inoise8(t * 90, y * 60, millis() / 2), 40);

            CRGB pixel = filamentColor;
            pixel.nscale8((uint8_t)(clamp01((0.14f + filamentTemp[t] * 0.86f) * glowShape) * 255.0f));
            pixel.nscale8_video(flicker);
            leds[idx(t, y)] = pixel;
        }
    }
}
