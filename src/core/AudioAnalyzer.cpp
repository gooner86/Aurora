#include "AudioAnalyzer.h"
#include "../audio/MicInput.h"
#include "../State.h"

#define FFT_SAMPLES 1024
int32_t rawSamples[FFT_SAMPLES];

// Helper: convert raw I2S sample to signed 32-bit sample for analysis.
// NOTE: The right-shift below reflects the source sample alignment used by MicInput.
// If you change microphone input format, update this conversion accordingly.
inline int32_t convertRawSample(int32_t r) {
    return r >> 14; // preserve existing behavior but make it explicit
}

namespace AudioAnalyzer {
    void update() {
        if (MicInput::readSamples(rawSamples, FFT_SAMPLES)) {
            double sumAbs = 0;
            
            for (int i = 0; i < FFT_SAMPLES; i++) {
                // Convert and remove DC/format alignment
                int32_t s = convertRawSample(rawSamples[i]);
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