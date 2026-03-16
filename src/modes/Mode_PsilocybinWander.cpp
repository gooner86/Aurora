#include "Mode_PsilocybinWander.h"
#include "../State.h"
#include "../Hardware.h"
#include <math.h>

namespace {
    struct WanderTheme {
        CRGB abyss;
        CRGB root;
        CRGB moss;
        CRGB bloom;
        CRGB ember;
        CRGB spore;
    };

    WanderTheme getTheme() {
        return {
            CRGB(4, 0, 10),
            CRGB(26, 12, 30),
            CRGB(52, 190, 128),
            CRGB(240, 138, 46),
            CRGB(222, 255, 246)
        };
    }
}

void Mode_PsilocybinWander::render() {
    static float driftPhase = 0.0f;
    static float branchPhase = 0.0f;
    static float sporePhase = 0.0f;
    static float focusTube = (TUBES - 1) * 0.5f;
    static float beatBloom = 0.0f;
    static float colorPhase = 0.0f;

    float wander = PSILO_WANDER / 100.0f;
    float bloom = PSILO_BLOOM / 100.0f;
    float lucidity = PSILO_LUCIDITY / 100.0f;
    WanderTheme theme = getTheme();

    float spectralMass = 0.0f;
    float spectralWeighted = 0.0f;
    for (int t = 0; t < TUBES; ++t) {
        float energy = (tubeBandFast(t) * 1.35f) + (tubeBand(t) * 0.85f);
        spectralMass += energy;
        spectralWeighted += energy * (float)t;
    }

    float targetFocus = (spectralMass > 0.001f)
        ? (spectralWeighted / spectralMass)
        : ((TUBES - 1) * 0.5f);
    focusTube = lerpf(focusTube, targetFocus, 0.06f + (wander * 0.16f));

    driftPhase += 0.14f + (bandBassS * (0.44f + wander * 1.30f)) + (volSmooth * 0.10f);
    branchPhase += 0.10f + (bandMidS * (0.34f + wander * 0.92f));
    sporePhase += 0.12f + (bandHighS * (0.48f + wander * 1.08f));
    colorPhase += 0.03f + (bandMidS * 0.08f) + (beatBloom * 0.02f);

    if (beatDetected) beatBloom = 1.0f;
    else beatBloom = lerpf(beatBloom, 0.0f, 0.09f);

    fill_solid(leds, NUM_LEDS, CRGB::Black);

    for (int t = 0; t < TUBES; ++t) {
        float localBand = tubeBand(t);
        float localFast = tubeBandFast(t);
        float localLift = tubeBandLevel(t);
        float localPeak = tubeBandPeak(t);
        float localField = tubeBandNeighborhood(t);

        float focus = clamp01(1.0f - (fabsf((float)t - focusTube) / lerpf(7.5f, 2.8f, wander)));
        focus = powf(focus, 1.30f);

        float course = sinf((driftPhase * 0.48f) + (t * lerpf(0.16f, 0.58f, wander)) + (localField * 2.9f));
        float swell = sinf((branchPhase * 0.90f) - (t * lerpf(0.10f, 0.32f, wander)) + (localBand * 3.6f));
        float fruiting = sinf(colorPhase + (t * 0.33f));
        uint8_t fruitTint = (uint8_t)constrain((int)lroundf(122.0f + (fruiting * 76.0f)), 0, 255);

        float threadYNorm = clamp01(
            0.46f
            + (course * (0.14f + wander * 0.17f))
            + (swell * (0.07f + wander * 0.10f))
            + (focus * 0.10f)
            + (bandBassS * 0.10f)
        );
        float threadY = lerpf(0.8f, H - 1.7f, threadYNorm);
        float branchY = constrain(
            threadY + (sinf(branchPhase + (t * 0.45f)) * (0.8f + wander * 1.5f)) - 0.7f + (focus * 0.55f),
            0.4f,
            (float)(H - 1)
        );
        float rootY = constrain(
            threadY - (1.2f + (cosf(driftPhase + (t * 0.28f)) * (0.45f + wander * 0.45f))),
            0.0f,
            (float)(H - 1)
        );

        float hazeWidth = lerpf(2.8f, 1.2f, lucidity);
        float threadWidth = lerpf(1.7f, 3.5f, bloom) + (localField * 1.0f) + (beatBloom * 0.8f);
        float branchWidth = threadWidth * (0.42f + bloom * 0.30f);
        float rootWidth = lerpf(1.0f, 2.1f, bloom);

        for (int y = 0; y < H; ++y) {
            float vertical = (float)y / (float)(H - 1);
            CRGB baseMist = blend(theme.root, theme.moss, (uint8_t)constrain((int)lroundf(50.0f + (fruiting * 40.0f) + (vertical * 70.0f)), 0, 255));
            CRGB color = blend(theme.abyss, baseMist, (uint8_t)(40 + (vertical * 60.0f) + (focus * 56.0f)));

            uint8_t curtain = inoise8(
                t * 42 + (int)(driftPhase * 16.0f),
                y * 24 - (int)(branchPhase * 16.0f),
                millis() / 11
            );
            CRGB atmosphere = blend(theme.abyss, theme.moss, qsub8(curtain, 84));
            nblend(color, atmosphere, 18 + (uint8_t)(localField * lerpf(120.0f, 64.0f, lucidity)));

            float hazeGlow = clamp01(1.0f - (fabsf((float)y - threadY) / (threadWidth + hazeWidth)));
            if (hazeGlow > 0.0f) {
                CRGB hazeColor = blend(theme.root, theme.moss, 120);
                hazeColor.nscale8((uint8_t)(clamp01(hazeGlow * (0.10f + bloom * 0.24f + localField * 0.20f) * (1.15f - lucidity * 0.45f)) * 255.0f));
                color += hazeColor;
            }

            float threadGlow = clamp01(1.0f - (fabsf((float)y - threadY) / threadWidth));
            threadGlow = powf(threadGlow, lerpf(1.15f, 2.15f, lucidity));
            if (threadGlow > 0.0f) {
                CRGB threadColor = blend(theme.moss, theme.bloom, (uint8_t)(110 + localFast * 110.0f));
                threadColor.nscale8((uint8_t)(clamp01(threadGlow * (0.34f + localBand * 0.52f + focus * 0.22f + beatBloom * 0.16f)) * 255.0f));
                color += threadColor;
            }

            float branchGlow = clamp01(1.0f - (fabsf((float)y - branchY) / branchWidth));
            branchGlow = powf(branchGlow, lerpf(1.0f, 1.95f, lucidity));
            if (branchGlow > 0.0f) {
                CRGB branchColor = blend(theme.moss, theme.ember, (uint8_t)(80 + localPeak * 120.0f));
                branchColor.nscale8((uint8_t)(clamp01(branchGlow * (0.11f + localField * 0.22f + focus * 0.18f)) * 255.0f));
                color += branchColor;
            }

            float rootGlow = clamp01(1.0f - (fabsf((float)y - rootY) / rootWidth));
            if (rootGlow > 0.0f) {
                CRGB rootColor = blend(theme.root, theme.ember, (uint8_t)(40 + beatBloom * 120.0f));
                rootColor.nscale8((uint8_t)(clamp01(rootGlow * (0.08f + localBand * 0.18f)) * 255.0f));
                color += rootColor;
            }

            int sporeThreshold = 250 - (int)(bandHighS * 60.0f) - (int)(localFast * 100.0f) - (int)(focus * 24.0f) - (int)(beatBloom * 20.0f);
            if (sporeThreshold < 176) sporeThreshold = 176;
            uint8_t sporeNoise = inoise8(
                t * 88 + 900,
                y * 88 + (int)(sporePhase * 30.0f),
                millis() / 6
            );
            if (sporeNoise > sporeThreshold && y >= (int)(threadY - 1.0f)) {
                CRGB spore = blend(theme.spore, theme.ember, fruitTint);
                spore.nscale8((uint8_t)(clamp01(((sporeNoise - sporeThreshold) / 55.0f) * (0.40f + lucidity * 0.70f)) * 255.0f));
                color += spore;
            }

            float alive = clamp01(
                0.10f
                + (localField * 0.22f)
                + (focus * 0.16f)
                + (hazeGlow * 0.16f)
                + (threadGlow * 0.34f)
                + (branchGlow * 0.14f)
                + (localLift * 0.12f)
                + (localPeak * 0.06f)
                + (volSmooth * 0.10f)
            );
            color.nscale8((uint8_t)(alive * 255.0f));
            leds[idx(t, y)] = color;
        }
    }
}
