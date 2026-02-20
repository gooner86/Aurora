#include "ModeManager.h"
#include "Transition.h"
#include "../State.h"
#include "../Hardware.h"
#include "../modes/ModeRegistry.h"

namespace ModeManager {

    // Updated to use 'int' to match State.h
    bool isAuroraFamily(int mode) {
        // Cases 0, 3, and 6 are Aurora V1, V2, and V3
        return (mode == 0 || mode == 3 || mode == 6);
    }

    void init() {
        // We don't force it to 0 here because SettingsManager 
        // will load the last saved mode into ACTIVE_MODE_INT.
        Serial.println("ModeManager Initialized");
    }

    void render() {
        // 1. Snapshot the current frame for the Transition engine 
        if (Transition::isTransitioning()) {
            for (int i = 0; i < NUM_LEDS; i++) {
                lastFrame[i] = leds[i];
            }
        }

        // 2. Run the actual Render logic for the selected mode
        switch (ACTIVE_MODE_INT) {
            case 0: Mode_AuroraV1::render();    break;
            case 1: Mode_EQSpectrum::render();  break; 
            case 2: Mode_VacuumTube::render();  break;
            case 3: Mode_AuroraV2::render();    break;
            case 4: Mode_RainbowFlow::render(); break;
            case 5: Mode_SolidColor::render();  break;
            case 6: Mode_AuroraV3::render();    break;
            case 7: Mode_NightRain::render();   break;
            case 8: Mode_DeepOcean::render();   break;
            case 9: Mode_IbizaSunset::render(); break;
            default: Mode_AuroraV1::render();   break;
        }

        // 3. Handle Crossfades (blends lastFrame into leds)
        Transition::applyCrossfade();

        // 4. Push to the hardware
        FastLED.show();
    }
}