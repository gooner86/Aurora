#pragma once
#include <Arduino.h>

namespace Transition {
    // Call this exactly once when the user changes the mode
    void trigger();
    
    // Call this at the very end of the render loop to blend the frames
    void applyCrossfade();
    
    // Check if a fade is currently happening
    bool isTransitioning();
}
