#pragma once
#include <Arduino.h>

namespace PhysicalControls {
    void init();
    void tick();
    void setBrightnessFromRemote(uint8_t val);
    void setSensitivityFromRemote(float sens);
}
