#pragma once
#include <Arduino.h>

namespace MicInput { // Or LineInput
    void init();
    bool readSamples(int32_t* buffer, size_t numSamples);
}
