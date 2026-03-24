#include "Mode_EQSpectrum.h"
#include "../State.h"

#include <math.h>

namespace Mode_EQSpectrum {
    void render() {
        static float fluidLevel[TUBES] = {0};
        static float fluidVelocity[TUBES] = {0};

        // Stretch the full sensitivity range so minimum barely tickles the floor
        // and maximum can happily drive the bars into the red.
        float sensNorm = clamp01((MASTER_SENSITIVITY - 0.05f) / (3.0f - 0.05f));
        float displaySens = powf(sensNorm, 0.52f);
        float deadZone = lerpf(0.48f, 0.01f, displaySens);
        float gain = lerpf(0.18f, 3.10f, displaySens);
        float responseCurve = lerpf(3.00f, 0.82f, displaySens);
        float springRise = lerpf(0.38f, 0.62f, displaySens);
        float springFall = lerpf(0.14f, 0.24f, displaySens);
        float dampingRise = lerpf(0.58f, 0.72f, displaySens);
        float dampingFall = lerpf(0.78f, 0.88f, displaySens);
        float settleRise = lerpf(0.18f, 0.28f, displaySens);
        float settleFall = lerpf(0.04f, 0.10f, displaySens);
        float bodyBrightness = lerpf(190.0f, 255.0f, displaySens);

        fill_solid(leds, NUM_LEDS, CRGB::Black);

        for (int t = 0; t < TUBES; ++t) {
            float smoothBand = tubeBandSplit(t);
            float fastBand = tubeBandSplitFast(t);
            float transient = max(0.0f, fastBand - smoothBand);
            float base = (smoothBand * 0.46f) + (transient * 0.90f);
            float raw = max(base, fastBand * 1.08f);

            float target = clamp01((raw - deadZone) * gain);
            target = powf(target, responseCurve);

            // Upward motion should feel pumped, while the fall should feel fluid and calm.
            bool rising = target > fluidLevel[t];
            float force = (target - fluidLevel[t]) * (rising ? springRise : springFall);
            fluidVelocity[t] = (fluidVelocity[t] + force) * (rising ? dampingRise : dampingFall);
            fluidLevel[t] = constrain(fluidLevel[t] + fluidVelocity[t], 0.0f, 1.08f);
            fluidLevel[t] = lerpf(fluidLevel[t], target, rising ? settleRise : settleFall);

            if (fluidLevel[t] < 0.002f && target < 0.002f && fabsf(fluidVelocity[t]) < 0.002f) {
                fluidLevel[t] = 0.0f;
                fluidVelocity[t] = 0.0f;
            }

            float surf = fluidLevel[t] * (float)H;
            for (int y = 0; y < H; ++y) {
                float depth = surf - (float)y;
                uint8_t hue = map(y, 0, H - 1, 96, 0);

                if (depth > -0.35f) {
                    float body = clamp01(depth);
                    float meniscus = 1.0f - clamp01(fabsf(depth) / 0.75f);
                    float brightness = (body * 0.78f) + (meniscus * 0.34f);
                    brightness = clamp01(brightness);

                    if (brightness > 0.0f) {
                        CRGB color = CHSV(hue, 255, (uint8_t)(bodyBrightness * brightness));
                        if (meniscus > 0.0f) {
                            CRGB cap = CHSV(qsub8(hue, 8), 210, (uint8_t)(150.0f * meniscus));
                            color += cap;
                        }
                        leds[idx(t, y)] = color;
                    }
                }
            }
        }
    }
}
