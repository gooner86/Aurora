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
        // Clamp to valid mode range (0-11)
        if (ACTIVE_MODE_INT < 0 || ACTIVE_MODE_INT > 11) ACTIVE_MODE_INT = 0;
        USER_BRIGHTNESS    = constrain((int)prefs.getUChar("bri", 90), 5, 255);
        MASTER_SENSITIVITY = constrain(prefs.getFloat("sens", 1.0f), 0.05f, 3.0f);
        globalBrightness   = USER_BRIGHTNESS;
        SOLID_STYLE        = prefs.getUChar("style", 0);
        PSILO_WANDER       = constrain((int)prefs.getUChar("psiWand", 72), 0, 100);
        PSILO_BLOOM        = constrain((int)prefs.getUChar("psiBloom", 64), 0, 100);
        PSILO_LUCIDITY     = constrain((int)prefs.getUChar("psiLucid", 58), 0, 100);
        
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
        prefs.putUChar("psiWand", PSILO_WANDER);
        prefs.putUChar("psiBloom", PSILO_BLOOM);
        prefs.putUChar("psiLucid", PSILO_LUCIDITY);
        
        uint32_t c = (SOLID_COLOR_VAL.r << 16) | (SOLID_COLOR_VAL.g << 8) | SOLID_COLOR_VAL.b;
        prefs.putUInt("color", c);
        
        Serial.println("System: Settings saved to NVS.");
    }
}
