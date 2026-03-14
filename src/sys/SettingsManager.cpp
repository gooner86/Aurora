#include "SettingsManager.h"
#include "../State.h"
#include <Preferences.h>

namespace SettingsManager {
    Preferences prefs;

    void init() {
        prefs.begin("aurora", false); // Open namespace "aurora"
        
        // Load variables with default fallbacks. 
        // ACTIVE_MODE_INT is now handled as a full integer.
        ACTIVE_MODE_INT    = prefs.getInt("mode", 1);
        // Clamp to valid mode range (0-9)
        if (ACTIVE_MODE_INT < 0 || ACTIVE_MODE_INT > 9) ACTIVE_MODE_INT = 0;
        USER_BRIGHTNESS    = prefs.getUChar("bri", 90);
        MASTER_SENSITIVITY = prefs.getFloat("sens", 1.0f);
        SOLID_STYLE        = prefs.getUChar("style", 0);
        
        uint32_t savedColor = prefs.getUInt("color", 0x00FFCC); 
        SOLID_COLOR_VAL = CRGB((savedColor >> 16) & 0xFF, (savedColor >> 8) & 0xFF, savedColor & 0xFF);
        
        Serial.println("System: Settings loaded from NVS.");
    }

    void save() {
        // We use putInt to match the 'int' type in State.h
        prefs.putInt("mode", ACTIVE_MODE_INT);
        prefs.putUChar("bri", USER_BRIGHTNESS);
        prefs.putFloat("sens", MASTER_SENSITIVITY);
        prefs.putUChar("style", SOLID_STYLE);
        
        uint32_t c = (SOLID_COLOR_VAL.r << 16) | (SOLID_COLOR_VAL.g << 8) | SOLID_COLOR_VAL.b;
        prefs.putUInt("color", c);
        
        Serial.println("System: Settings saved to NVS.");
    }
}