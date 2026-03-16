#include "Mode_SolidColor.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_SolidColor::render() {
    static float driftA = 0.0f;
    static float driftB = 0.0f;
    bool curtainStyle = (SOLID_STYLE == 0);
    driftA += (curtainStyle ? 0.24f : 0.18f) + (bandBassS * (curtainStyle ? 0.80f : 0.58f));
    driftB += (curtainStyle ? 0.14f : 0.09f) + (bandMidS * (curtainStyle ? 0.55f : 0.38f));

    CRGB tint = SOLID_COLOR_VAL;
    if ((uint16_t)tint.r + (uint16_t)tint.g + (uint16_t)tint.b < 16) {
        tint = CRGB(0, 255, 204);
    }

    CRGB shadow = tint;
    shadow.nscale8(24);
    CRGB glow = tint;
    glow.nscale8(150);
    CRGB pearl = blend(glow, CRGB(255, 245, 230), 96);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int t = 0; t < TUBES; t++) {
        float localBand = tubeBand(t);
        float localFast = tubeBandFast(t);
        float localLift = tubeBandLevel(t);
        float localField = tubeBandNeighborhood(t);
        float crest = 1.0f + (localLift * (H - 2.5f));

        for (int y = 0; y < H; y++) {
            uint8_t field = inoise8(
                t * 50 + (int)(driftA * 16.0f),
                y * 24 - (int)(driftB * 22.0f),
                millis() / 10
            );
            uint8_t detail = inoise8(
                t * 75 + 1200,
                y * 30 + (int)(driftA * 11.0f),
                millis() / 14
            );

            float haze = curtainStyle
                ? clamp01(0.08f + (field / 255.0f) * 0.22f + (localField * 0.42f))
                : clamp01(0.05f + (field / 255.0f) * 0.16f + (localBand * 0.28f) + (localField * 0.18f));
            CRGB color = blend(shadow, glow, (uint8_t)(haze * 255.0f));

            float dist = fabsf(y - crest);
            float crestSpan = curtainStyle ? 3.3f : 2.4f;
            if (dist < crestSpan) {
                float core = powf(1.0f - (dist / crestSpan), curtainStyle ? 1.8f : 1.25f);
                CRGB highlight = pearl;
                highlight.nscale8((uint8_t)(clamp01(core * (curtainStyle ? (0.24f + localFast * 0.85f) : (0.16f + localFast * 1.05f))) * 255.0f));
                color += highlight;
            }

            if (!curtainStyle) {
                float body = clamp01(1.0f - (fabsf(y - (crest - 0.6f)) / 4.6f));
                CRGB columnGlow = blend(glow, pearl, 72);
                columnGlow.nscale8((uint8_t)(clamp01(body * (0.08f + localLift * 0.60f)) * 255.0f));
                color += columnGlow;
            }

            float alive = curtainStyle
                ? clamp01(0.16f + (localBand * 0.40f) + (localField * 0.22f) + ((detail / 255.0f) * 0.08f))
                : clamp01(0.12f + (localBand * 0.32f) + (localLift * 0.40f) + ((detail / 255.0f) * 0.05f));
            color.nscale8((uint8_t)(alive * 255.0f));
            leds[idx(t, y)] = color;
        }
    }
}
