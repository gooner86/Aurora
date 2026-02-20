#pragma once
#include <Arduino.h>

namespace ModeManager {
    // Initialize the mode state
    void init();
    
    // The master function that decides which mode to draw and handles the final output
    void render();
    
    // Helper to determine if we should use the "Aurora" style blending
    bool isAuroraFamily(int mode);
}