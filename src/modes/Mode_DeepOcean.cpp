#include "Mode_DeepOcean.h"
#include "../State.h"
#include "../Hardware.h"
#include <math.h>

namespace {
    struct OceanTheme {
        CRGB trench;
        CRGB midwater;
        CRGB current;
        CRGB crest;
        CRGB foam;
        CRGB glint;
    };

    OceanTheme getTheme() {
        return {
            CRGB(0, 4, 18),
            CRGB(0, 26, 72),
            CRGB(0, 110, 136),
            CRGB(28, 212, 194),
            CRGB(196, 248, 255),
            CRGB(70, 168, 255)
        };
    }
}

namespace Mode_DeepOcean {
    void render() {
        static float tidePhase = 0.0f;
        static float currentPhase = 0.0f;
        static float shimmerPhase = 0.0f;
        static float foamPulse = 0.0f;

        OceanTheme theme = getTheme();
        uint32_t now = millis();

        tidePhase += tempoRate(0.04f, 0.18f) + (bandBassS * 0.16f) + (volSmooth * 0.03f);
        currentPhase += tempoRate(0.05f, 0.22f) + (bandMidS * 0.22f) + (volSmooth * 0.03f);
        shimmerPhase += tempoRate(0.06f, 0.28f) + (bandHighS * 0.32f) + (volBeat * 0.08f);

        if (beatDetected) {
            foamPulse = 1.0f;
        } else {
            foamPulse = lerpf(foamPulse, 0.0f, 0.08f);
        }

        for (int t = 0; t < TUBES; t++) {
            float localBand = tubeBand(t);
            float localFast = tubeBandFast(t);
            float localLevel = tubeBandLevel(t);
            float localPeak = tubeBandPeak(t);
            float localField = tubeBandNeighborhood(t);
            float lateral = (TUBES > 1) ? ((float)t / (float)(TUBES - 1)) : 0.0f;

            uint8_t surfaceNoise = inoise8(
                t * 40 + (int)(tidePhase * 22.0f),
                320 + (int)(currentPhase * 12.0f),
                now / 15
            );
            uint8_t crossNoise = inoise8(
                t * 24 + 900,
                1180 + (int)(currentPhase * 16.0f),
                now / 18
            );
            uint8_t undertowNoise = inoise8(
                t * 36 + 1500,
                260 + (int)(currentPhase * 18.0f),
                now / 11
            );
            float surfaceDrift = ((float)surfaceNoise - 128.0f) / 128.0f;
            float crossDrift = ((float)crossNoise - 128.0f) / 128.0f;
            float undertowDrift = ((float)undertowNoise - 128.0f) / 128.0f;

            float surfaceNorm = clamp01(
                0.46f
                + (surfaceDrift * 0.14f)
                + (crossDrift * 0.07f)
                + (localField * 0.22f)
                + (bandBassS * 0.08f)
            );
            float surfaceY = lerpf(3.0f, H - 1.2f, surfaceNorm);
            float swellWidth = 1.20f + (localField * 1.75f) + (bandBassS * 0.75f);

            float undertowNorm = clamp01(
                0.22f
                + (lateral * 0.18f)
                + ((undertowDrift + 1.0f) * 0.16f)
                + (localLevel * 0.10f)
            );
            float undertowY = lerpf(0.4f, H - 3.2f, undertowNorm);
            float undertowWidth = 1.50f + (bandBassS * 1.35f) + (localBand * 0.65f);

            for (int y = 0; y < H; y++) {
                float vertical = (float)y / (float)(H - 1);
                float topBias = powf(vertical, 1.35f);

                CRGB color = blend(theme.trench, theme.midwater, (uint8_t)(36 + (vertical * 156.0f)));
                nblend(color, theme.current, (uint8_t)(10 + (topBias * 90.0f)));

                uint8_t currentNoise = inoise8(
                    t * 46 + (int)(currentPhase * 18.0f),
                    y * 34 - (int)(tidePhase * 12.0f),
                    now / 13
                );
                CRGB currentColor = blend(theme.midwater, theme.current, qadd8(currentNoise, (uint8_t)(topBias * 90.0f)));
                nblend(color, currentColor, (uint8_t)(18 + (localField * 92.0f)));

                uint8_t causticNoise = inoise8(
                    t * 84 + 1200,
                    y * 60 + (int)(shimmerPhase * 18.0f),
                    now / 8
                );
                float caustic = clamp01(((float)causticNoise - 104.0f) / 150.0f) * topBias;
                if (caustic > 0.0f) {
                    CRGB glint = blend(theme.current, theme.glint, (uint8_t)(90 + (localFast * 100.0f)));
                    glint.nscale8((uint8_t)(clamp01(caustic * (0.18f + (localField * 0.16f))) * 255.0f));
                    color += glint;
                }

                float undertowGlow = clamp01(1.0f - (fabsf((float)y - undertowY) / undertowWidth));
                if (undertowGlow > 0.0f) {
                    CRGB undertowColor = blend(theme.midwater, theme.current, (uint8_t)(40 + (lateral * 80.0f)));
                    undertowColor.nscale8((uint8_t)(clamp01(undertowGlow * (0.10f + (bandBassS * 0.26f) + (localBand * 0.12f))) * 255.0f));
                    color += undertowColor;
                }

                float swellGlow = clamp01(1.0f - (fabsf((float)y - surfaceY) / swellWidth));
                swellGlow = powf(swellGlow, 1.35f);
                if (swellGlow > 0.0f) {
                    CRGB swellColor = blend(theme.current, theme.crest, (uint8_t)(80 + (localBand * 120.0f)));
                    swellColor.nscale8((uint8_t)(clamp01(swellGlow * (0.26f + (localField * 0.30f) + (volSmooth * 0.08f))) * 255.0f));
                    color += swellColor;
                }

                float crestDistance = fabsf((float)y - surfaceY);
                if (crestDistance < 1.7f) {
                    int foamThreshold = 246
                        - (int)(bandHighS * 72.0f)
                        - (int)(localFast * 120.0f)
                        - (int)(foamPulse * 24.0f);
                    if (foamThreshold < 168) foamThreshold = 168;

                    uint8_t foamNoise = inoise8(
                        t * 92 + 1500,
                        y * 92 - (int)(shimmerPhase * 34.0f),
                        now / 5
                    );
                    if (foamNoise > foamThreshold && y >= (int)(surfaceY - 1.0f)) {
                        float foamAmount = clamp01(((float)foamNoise - (float)foamThreshold) / 64.0f);
                        CRGB foam = blend(theme.foam, theme.glint, (uint8_t)(40 + (localPeak * 110.0f)));
                        foam.nscale8((uint8_t)(clamp01(foamAmount * (0.38f + (localFast * 0.46f) + (bandHighS * 0.22f))) * 255.0f));
                        color += foam;
                    }
                }

                float alive = clamp01(
                    0.26f
                    + (topBias * 0.14f)
                    + (caustic * 0.12f)
                    + (undertowGlow * 0.10f)
                    + (swellGlow * 0.10f)
                );
                color.nscale8_video((uint8_t)(alive * 255.0f));
                leds[idx(t, y)] = color;
            }
        }
    }
}
