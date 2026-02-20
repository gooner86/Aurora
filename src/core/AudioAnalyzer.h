#pragma once
#include <Arduino.h>

namespace AudioAnalyzer {
    // This MUST be here for main.cpp to see it
    void init(); 
    
    // The main loop that runs the FFT
    void update();
}