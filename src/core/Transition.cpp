#include "Transition.h"
#include "../State.h"
#include "../Hardware.h"
#include <FastLED.h>

namespace Transition {
    
    // A hidden canvas to store the frozen "snapshot" of the old mode
    CRGB snapshotBuffer[NUM_LEDS];
    
    uint32_t transitionStartTime = 0;
    const uint32_t FADE_DURATION_MS = 1000; // 1-second crossfade
    bool active = false;

    void trigger() {
        // Take a high-speed photo of whatever is currently on the LEDs
        for(int i = 0; i < NUM_LEDS; i++) {
            snapshotBuffer[i] = leds[i];
        }
        transitionStartTime = millis();
        active = true;
    }

    bool isTransitioning() {
        return active;
    }

    void applyCrossfade() {
        if (!active) return;

        uint32_t elapsed = millis() - transitionStartTime;
        
        // If 1 second has passed, the transition is over
        if (elapsed >= FADE_DURATION_MS) {
            active = false;
            return;
        }

        // Calculate fade amount: 
        // 255 = fully old snapshot, 0 = fully new live mode
        uint8_t fadeAmount = 255 - (uint8_t)((elapsed * 255) / FADE_DURATION_MS);

        // Blend the frozen snapshot over the newly rendering frame
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = blend(leds[i], snapshotBuffer[i], fadeAmount);
        }
    }
}
