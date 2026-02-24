#include "AudioAnalyzer.h"
#include "../audio/MicInput.h"
#include "../State.h"

#define FFT_SAMPLES 1024
int32_t rawSamples[FFT_SAMPLES];

namespace AudioAnalyzer {
    void update() {
        if (MicInput::readSamples(rawSamples, FFT_SAMPLES)) {
            double sumAbs = 0;
            
            for (int i = 0; i < FFT_SAMPLES; i++) {
                // Remove the DC offset and shift
                int32_t s = rawSamples[i] >> 14; 
                sumAbs += abs(s);
            }

            float avg = (float)(sumAbs / FFT_SAMPLES);
            
            // Temporary high-gain for debugging
            float v = (avg * MASTER_SENSITIVITY) / 500.0f; 
            
            volSmooth = (volSmooth * 0.8f) + (v * 0.2f);

            static uint32_t lastD = 0;
            if (millis() - lastD > 300) {
                // If this is still 0, the mic is definitely not getting a clock signal
                Serial.printf("MIC CHECK | Raw Sample: %ld | volSmooth: %.4f\n", rawSamples[0], volSmooth);
                lastD = millis();
            }
        }
    }
}