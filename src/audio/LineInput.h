#pragma once
#include <Arduino.h>

namespace LineInput {
    // Lets other files know if the Line-In board successfully booted
    extern bool lineOk;
    
    // The setup function
    void init();
    
    // The function that grabs the raw audio data
    bool readSamples(int32_t* buffer, size_t numSamples);
}
