#include "Mode_VacuumTube.h"
#include "../State.h"
#include "../Hardware.h"

void Mode_VacuumTube::render() {
    // bandBass8 comes from your AudioAnalyzer
    uint8_t warmth = map(bandBass8, 0, 255, 100, 255); 
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t flicker = qsub8(inoise8(i * 50, millis() / 2), 50);
        leds[i] = CRGB(warmth, warmth / 3, 0); // Amber glow
        leds[i].nscale8_video(flicker);
    }
}
