#include "ModeManager.h"
#include "Transition.h"
#include "../State.h"
#include "../Hardware.h"
#include "../modes/ModeRegistry.h"

namespace ModeManager {
    static bool frameHistoryValid = false;

    // Updated to use 'int' to match State.h
    bool isAuroraFamily(int mode) {
        return (mode == 0 || mode == 2 || mode == 3 || mode == 5 || mode == 6 || mode == 7 || mode == 8 || mode == 9 || mode == 10);
    }

    void init() {
        // We don't force it to 0 here because SettingsManager 
        // will load the last saved mode into ACTIVE_MODE_INT.
        Serial.println("ModeManager Initialized");
    }

    void render() {
        if (currentPower == PowerState::STANDBY) {
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            for (int i = 0; i < NUM_LEDS; ++i) {
                lastFrame[i] = CRGB::Black;
            }
            frameHistoryValid = false;
            return;
        }

        // Ensure mode is valid to avoid out-of-range lookups
        if (ACTIVE_MODE_INT < 0 || ACTIVE_MODE_INT > 11) {
            ACTIVE_MODE_INT = 0;
        }
        // 1. Run the actual Render logic for the selected mode
        switch (ACTIVE_MODE_INT) {
            case 0: Mode_AuroraV1::render();    break;
            case 1: Mode_EQSpectrum::render();  break; 
            case 2: Mode_VacuumTube::render();  break;
            case 3: Mode_AuroraV2::render();    break;
            case 4: Mode_RainbowFlow::render(); break; // Party / showcase mode
            case 5: Mode_SolidColor::render();  break; // Color Veil
            case 6: Mode_AuroraV3::render();    break;
            case 7: Mode_NightRain::render();   break;
            case 8: Mode_DeepOcean::render();   break;
            case 9: Mode_IbizaSunset::render(); break;
            case 10: Mode_PsilocybinWander::render(); break;
            case 11: Mode_TempoTest::render(); break;
            default: Mode_AuroraV1::render();   break;
        }

        // 2. Handle mode-change crossfades
        Transition::applyCrossfade();

        // 3. Signature ambient modes keep a gentle frame memory.
        if (isAuroraFamily(ACTIVE_MODE_INT)) {
            if (!frameHistoryValid) {
                for (int i = 0; i < NUM_LEDS; i++) {
                    lastFrame[i] = leds[i];
                }
                frameHistoryValid = true;
                return;
            }

            const float blendK = 0.38f;
            for (int i = 0; i < NUM_LEDS; i++) {
                leds[i].r = (uint8_t)(lastFrame[i].r + (leds[i].r - lastFrame[i].r) * blendK);
                leds[i].g = (uint8_t)(lastFrame[i].g + (leds[i].g - lastFrame[i].g) * blendK);
                leds[i].b = (uint8_t)(lastFrame[i].b + (leds[i].b - lastFrame[i].b) * blendK);
                lastFrame[i] = leds[i];
            }
        } else {
            for (int i = 0; i < NUM_LEDS; i++) {
                lastFrame[i] = leds[i];
            }
            frameHistoryValid = true;
        }
    }
}
