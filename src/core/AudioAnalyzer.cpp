#include "AudioAnalyzer.h"
#include "../audio/MicInput.h"
#include "../State.h"
#include <arduinoFFT.h>

#define FFT_SAMPLES 1024
double vReal[FFT_SAMPLES], vImag[FFT_SAMPLES];
// Note: If you have arduinoFFT v2.0, use the newer syntax if needed, 
// but this usually works for legacy compatibility.
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, FFT_SAMPLES, 44100);
int32_t rawSamples[FFT_SAMPLES];

namespace AudioAnalyzer {

    // V32 Sign Extension
    static inline int32_t signExtend24(int32_t v) {
        if (v & 0x00800000) v |= 0xFF000000;
        return v;
    }

    // V32 Mic Sampling
    static inline int32_t micSample24(int32_t w) {
        int32_t a = signExtend24((uint32_t)w >> 8);
        int32_t b = signExtend24((uint32_t)w & 0x00FFFFFF);
        int32_t c = (int32_t)((int16_t)((uint32_t)w >> 16)) << 8;
        int32_t aa = abs(a), bb = abs(b), cc = abs(c);
        if (aa <= bb && aa <= cc) return a;
        if (bb <= aa && bb <= cc) return b;
        return c;
    }

    void update() {
        if (MicInput::readSamples(rawSamples, FFT_SAMPLES)) {
            double sumAbs = 0;
            for (int i = 0; i < FFT_SAMPLES; i++) {
                int32_t s = micSample24(rawSamples[i]);
                vReal[i] = (double)s;
                vImag[i] = 0.0;
                sumAbs += fabs((double)s);
            }

            FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
            FFT.compute(FFT_FORWARD);
            FFT.complexToMagnitude();

            float avgAbs = (float)(sumAbs / FFT_SAMPLES);
            float sens = (MASTER_SENSITIVITY * MASTER_SENSITIVITY);
            float v = (avgAbs * sens) / 3000000.0f;
            
            // Smoother reactivity
            volSmooth = (volSmooth * 0.82f) + (v * 0.18f);
        }
    }
}