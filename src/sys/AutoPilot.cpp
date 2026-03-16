#include "AutoPilot.h"
#include "../State.h"
#include "../core/Transition.h"
#include "SettingsManager.h"

namespace AutoPilot {
    uint32_t lastSwitchTime = 0;
    const uint32_t CYCLE_TIME_MS = 30000; // Switch every 30s
    bool armed = false;

    // Playlist adjusted to match the 11 modes defined in ModeManager (0-10)
    uint8_t playlist[] = {0, 3, 6, 10, 8, 9, 5, 2, 7, 1, 4}; 
    uint8_t playlistIndex = 0;

    void tick() {
        if (currentPlayMode != PlayMode::AUTOPILOT) return;

        if (millis() - lastSwitchTime > CYCLE_TIME_MS) {
            armed = true; 
        }

        // Only switch when a beat is detected for maximum impact
        if (armed && beatDetected) {
            Transition::trigger(); 
            
            playlistIndex = (playlistIndex + 1) % (sizeof(playlist) / sizeof(playlist[0]));
            ACTIVE_MODE_INT = playlist[playlistIndex];
            
            // Request a debounced save so we don't write NVS from this context
            settingsDirty = true;
            
            lastSwitchTime = millis();
            armed = false;
            Serial.print("AutoPilot: Switching to mode ");
            Serial.println(ACTIVE_MODE_INT);
        }
    }
}
