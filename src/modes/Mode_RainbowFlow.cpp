#include "Mode_RainbowFlow.h"
#include "../State.h"
#include "../Hardware.h"
#include <math.h>

void Mode_RainbowFlow::render() {
    static float riverPhase = 0.0f;
    static float shearPhase = 0.0f;
    static float huePhase = 0.0f;
    static float glintPhase = 0.0f;

    uint32_t now = millis();

    huePhase += 0.32f + (bandHighS * 0.18f);
    riverPhase += 0.08f + (bandBassS * 0.62f) + (volSmooth * 0.04f);
    shearPhase += 0.10f + (bandMidS * 0.72f) + (volBeat * 0.05f);
    glintPhase += 0.18f + (bandHighS * 1.10f) + (volBeat * 0.10f);

    for (int t = 0; t < TUBES; t++) {
        float localBand = tubeBand(t);
        float localFast = tubeBandFast(t);
        float localPeak = tubeBandPeak(t);
        float localField = tubeBandNeighborhood(t);
        float lateral = (TUBES > 1) ? ((float)t / (float)(TUBES - 1)) : 0.0f;

        float riverNorm = clamp01(
            0.48f
            + (sinf((lateral * 5.6f) + riverPhase) * 0.18f)
            + (sinf((lateral * 2.1f) - (shearPhase * 0.80f)) * 0.10f)
            + ((localField - 0.35f) * 0.16f)
            + (bandBassS * 0.06f)
        );
        float riverY = lerpf(1.2f, H - 1.3f, riverNorm);
        float riverWidth = 2.0f + (bandBassS * 1.05f) + (localField * 1.20f);

        float seamY = constrain(
            riverY + (sinf((lateral * 4.0f) + shearPhase) * (0.55f + bandMidS * 1.0f)),
            0.2f,
            (float)(H - 1)
        );
        float seamWidth = 0.90f + (bandHighS * 0.75f) + (localFast * 0.60f);

        for (int y = 0; y < H; y++) {
            float vertical = (float)y / (float)(H - 1);

            uint8_t flowNoise = inoise8(
                t * 42 + (int)(riverPhase * 18.0f),
                y * 28 - (int)(shearPhase * 20.0f),
                now / 10
            );
            float flow = clamp01(((float)flowNoise - 56.0f) / 180.0f);

            uint8_t curlNoise = inoise8(
                t * 68 + 1200 + (int)(shearPhase * 16.0f),
                y * 44 + (int)(riverPhase * 10.0f),
                now / 7
            );
            float curl = clamp01(((float)curlNoise - 86.0f) / 150.0f);

            float bodyGlow = clamp01(1.0f - (fabsf((float)y - riverY) / riverWidth));
            bodyGlow = powf(bodyGlow, 1.20f);

            float seamGlow = clamp01(1.0f - (fabsf((float)y - seamY) / seamWidth));
            seamGlow = powf(seamGlow, 1.65f);

            float edgeGlow = clamp01(1.0f - (fabsf(fabsf((float)y - riverY) - (riverWidth * 0.72f)) / (0.70f + bandHighS * 0.55f)));
            edgeGlow = powf(edgeGlow, 1.40f);

            uint8_t hue = (uint8_t)(
                huePhase
                + (lateral * 122.0f)
                + (vertical * 46.0f)
                + (sinf((vertical * 6.5f) + (lateral * 7.5f) + shearPhase) * 28.0f)
                + (flow * 44.0f)
                + (curl * 24.0f)
            );

            CRGB color = CHSV(hue + 150, 255, (uint8_t)(4 + flow * 12.0f));

            if (bodyGlow > 0.0f) {
                uint8_t bodyHue = hue + (uint8_t)(curl * 36.0f);
                CRGB body = CHSV(bodyHue, 250, (uint8_t)(bodyGlow * (125.0f + localField * 85.0f + flow * 55.0f)));
                color += body;
            }

            if (seamGlow > 0.0f) {
                uint8_t seamHue = hue + 96 + (uint8_t)(localFast * 18.0f);
                CRGB seam = CHSV(seamHue, 235, (uint8_t)(seamGlow * (115.0f + bandMidS * 90.0f + localBand * 50.0f)));
                color += seam;
            }

            if (edgeGlow > 0.0f) {
                uint8_t edgeHue = hue + 170;
                CRGB edge = CHSV(edgeHue, 190, (uint8_t)(edgeGlow * (110.0f + bandHighS * 85.0f + localPeak * 35.0f)));
                color += edge;
            }

            uint8_t glintNoise = inoise8(
                t * 94 + 2100,
                y * 94 + (int)(glintPhase * 26.0f),
                now / 5
            );
            int glintThreshold = 252 - (int)(bandHighS * 70.0f) - (int)(localFast * 90.0f) - (int)(edgeGlow * 26.0f);
            if (glintThreshold < 178) glintThreshold = 178;
            if (glintNoise > glintThreshold && (bodyGlow > 0.12f || edgeGlow > 0.18f)) {
                float glint = clamp01(((float)glintNoise - (float)glintThreshold) / 64.0f);
                CRGB sparkle = CHSV(hue + 205, 110, (uint8_t)(glint * (120.0f + bandHighS * 110.0f)));
                color += sparkle;
            }

            float alive = clamp01(
                0.05f
                + (flow * 0.08f)
                + (curl * 0.10f)
                + (bodyGlow * 0.40f)
                + (seamGlow * 0.22f)
                + (edgeGlow * 0.18f)
                + (localField * 0.08f)
                + (volSmooth * 0.04f)
            );
            color.nscale8((uint8_t)(alive * 255.0f));
            leds[idx(t, y)] = color;
        }
    }
}
