#include "AudioAnalyzer.h"
#include "../audio/LineInput.h"
#include "../audio/MicInput.h"
#include "../State.h"
#include <arduinoFFT.h>
#include <math.h>

#define FFT_SAMPLES 1024

static int32_t rawSamples[FFT_SAMPLES];
static double vReal[FFT_SAMPLES];
static double vImag[FFT_SAMPLES];
static ArduinoFFT<double> FFT(vReal, vImag, FFT_SAMPLES, 44100.0);
static uint32_t lastAudioMs = 0;
static float audioFloor = 20000.0f;
static float audioCeiling = 180000.0f;
static int bandEdges[TUBES + 1];
static bool bandEdgesReady = false;
static float bandFloor[TUBES];
static float bandCeiling[TUBES];
static float bandWeight[TUBES];

static inline float smoothAR(float cur, float target, float attack, float release) {
    float k = (target > cur) ? attack : release;
    return cur + (target - cur) * k;
}

static inline int32_t decodeAudioSample(int32_t raw) {
    if (currentAudio == AudioSource::MIC_IN) {
        return MicInput::decodeSample(raw);
    }
    return raw >> 8;
}

static bool readActiveSamples() {
    switch (currentAudio) {
        case AudioSource::MIC_IN:
            return MicInput::readSamples(rawSamples, FFT_SAMPLES);
        case AudioSource::LINE_IN:
            return LineInput::readSamples(rawSamples, FFT_SAMPLES);
        case AudioSource::OFF:
        default:
            return false;
    }
}

static void initBandEdges() {
    const int minBin = 2; // Skip the DC/rumble bin that tends to pin the first bar.
    const int maxBin = (FFT_SAMPLES / 2) - 1;
    const int minWidth = 2;
    bandEdges[0] = minBin;

    int prev = minBin;
    for (int i = 1; i < TUBES; ++i) {
        float t = (float)i / (float)TUBES;
        int edge = (int)lroundf((float)minBin * powf((float)maxBin / (float)minBin, t));
        if (edge < prev + minWidth) {
            edge = prev + minWidth;
        }
        int maxAllowed = maxBin - (minWidth * (TUBES - i));
        if (edge > maxAllowed) {
            edge = maxAllowed;
        }
        bandEdges[i] = edge;
        prev = edge;
    }

    bandEdges[TUBES] = maxBin;
    for (int t = 0; t < TUBES; ++t) {
        float pos = (TUBES > 1) ? ((float)t / (float)(TUBES - 1)) : 0.0f;
        bandWeight[t] = lerpf(0.78f, 1.18f, pos);
    }
    bandEdgesReady = true;
}

static void decayToSilence() {
    for (int t = 0; t < TUBES; ++t) {
        tubeBandsInstant[t] = 0.0f;
        tubeBandsSmooth[t] = smoothAR(tubeBandsSmooth[t], 0.0f, 0.10f, 0.18f);
    }

    bandBassS = smoothAR(bandBassS, 0.0f, 0.12f, 0.18f);
    bandMidS = smoothAR(bandMidS, 0.0f, 0.12f, 0.18f);
    bandHighS = smoothAR(bandHighS, 0.0f, 0.12f, 0.18f);
    volSmooth = smoothAR(volSmooth, 0.0f, 0.12f, 0.18f);
    volBeat = smoothAR(volBeat, 0.0f, 0.18f, 0.25f);
    levelFast = smoothAR(levelFast, 0.0f, 0.18f, 0.25f);
    levelSlow = smoothAR(levelSlow, 0.0f, 0.08f, 0.14f);
    bassEnergy = 0.0f;
    midEnergy = 0.0f;
    highEnergy = 0.0f;
    beatDetected = false;
}

namespace AudioAnalyzer {
    void init() {
        for (int i = 0; i < TUBES; ++i) {
            tubeMax[i] = 0.0f;
            bandFloor[i] = 0.0f;
            bandCeiling[i] = 0.0f;
        }
        audioFloor = 20000.0f;
        audioCeiling = 180000.0f;
        initBandEdges();
    }

    void update() {
        uint32_t now = millis();
        if ((uint32_t)(now - lastAudioMs) < 15) {
            return;
        }
        lastAudioMs = now;

        if (!readActiveSamples()) {
            decayToSilence();
            return;
        }

        double sum = 0.0;
        for (int i = 0; i < FFT_SAMPLES; ++i) {
            sum += (double)decodeAudioSample(rawSamples[i]);
        }
        float mean = (float)(sum / FFT_SAMPLES);

        double sumAbsDev = 0.0;
        for (int i = 0; i < FFT_SAMPLES; ++i) {
            float centered = (float)decodeAudioSample(rawSamples[i]) - mean;
            vReal[i] = (double)centered;
            vImag[i] = 0.0;
            sumAbsDev += fabs(centered);
        }

        float avgAbsDev = (float)(sumAbsDev / FFT_SAMPLES);

        FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
        FFT.compute(FFTDirection::Forward);
        FFT.complexToMagnitude();
        vReal[0] = 0.0;
        vReal[1] = 0.0;

        if (avgAbsDev < audioFloor) {
            audioFloor = lerpf(audioFloor, avgAbsDev, 0.12f);
        } else {
            audioFloor = lerpf(audioFloor, avgAbsDev, 0.0025f);
        }

        float desiredCeiling = max((avgAbsDev * 2.8f) + 2000.0f, (audioFloor * 2.6f) + 4000.0f);
        if (desiredCeiling > audioCeiling) {
            audioCeiling = lerpf(audioCeiling, desiredCeiling, 0.16f);
        } else {
            audioCeiling = lerpf(audioCeiling, desiredCeiling, 0.025f);
        }

        if (audioFloor < 2000.0f) {
            audioFloor = 2000.0f;
        }
        if (audioCeiling < audioFloor * 2.4f) {
            audioCeiling = audioFloor * 2.4f;
        }

        float gated = avgAbsDev - (audioFloor * 1.05f);
        if (gated < 0.0f) {
            gated = 0.0f;
        }

        float dynamicRange = audioCeiling - audioFloor;
        if (dynamicRange < 8000.0f) {
            dynamicRange = 8000.0f;
        }

        float normalized = clamp01((gated / dynamicRange) * MASTER_SENSITIVITY);
        float inputLevel = powf(normalized, 0.60f);
        float presenceGate = clamp01(0.04f + (inputLevel * 1.08f));

        volSmooth = smoothAR(volSmooth, inputLevel, 0.18f, 0.06f);
        volBeat = smoothAR(volBeat, inputLevel, 0.45f, 0.12f);

        float totalEnergy = 0.0f;
        float bassSum = 0.0f;
        float midSum = 0.0f;
        float highSum = 0.0f;
        int bassCount = 0;
        int midCount = 0;
        int highCount = 0;

        for (int t = 0; t < TUBES; ++t) {
            double bandSum = 0.0;
            for (int b = bandEdges[t]; b < bandEdges[t + 1]; ++b) {
                bandSum += vReal[b];
            }

            int bandBins = bandEdges[t + 1] - bandEdges[t];
            if (bandBins < 1) {
                bandBins = 1;
            }

            float bandLevel = ((float)bandSum / (float)bandBins) * bandWeight[t];
            if (bandLevel < 0.0f) {
                bandLevel = 0.0f;
            }

            if (bandFloor[t] <= 0.0f && bandCeiling[t] <= 0.0f) {
                bandFloor[t] = bandLevel;
                bandCeiling[t] = max((bandLevel * 3.0f) + 120.0f, 320.0f);
            }

            if (bandLevel < bandFloor[t]) {
                bandFloor[t] = lerpf(bandFloor[t], bandLevel, 0.12f);
            } else {
                bandFloor[t] = lerpf(bandFloor[t], bandLevel, 0.0020f);
            }

            float desiredBandCeiling = max((bandLevel * 2.6f) + 120.0f, (bandFloor[t] * 2.8f) + 160.0f);
            if (desiredBandCeiling > bandCeiling[t]) {
                bandCeiling[t] = lerpf(bandCeiling[t], desiredBandCeiling, 0.16f);
            } else {
                bandCeiling[t] = lerpf(bandCeiling[t], desiredBandCeiling, 0.03f);
            }

            float bandRange = bandCeiling[t] - bandFloor[t];
            if (bandRange < 200.0f) {
                bandRange = 200.0f;
            }

            float bandGated = bandLevel - (bandFloor[t] * 1.08f);
            if (bandGated < 0.0f) {
                bandGated = 0.0f;
            }

            float bandNormalized = clamp01((bandGated / bandRange) * MASTER_SENSITIVITY);
            bandNormalized = powf(bandNormalized, 0.72f) * presenceGate;
            tubeBandsInstant[t] = bandNormalized;
            tubeBandsSmooth[t] = smoothAR(tubeBandsSmooth[t], bandNormalized, 0.35f, 0.12f);
            tubeMax[t] = bandCeiling[t];
            totalEnergy += tubeBandsSmooth[t];

            if (t < (TUBES / 3)) {
                bassSum += tubeBandsSmooth[t];
                bassCount++;
            } else if (t < ((TUBES * 2) / 3)) {
                midSum += tubeBandsSmooth[t];
                midCount++;
            } else {
                highSum += tubeBandsSmooth[t];
                highCount++;
            }
        }

        bassEnergy = (bassCount > 0) ? (bassSum / (float)bassCount) : 0.0f;
        midEnergy = (midCount > 0) ? (midSum / (float)midCount) : 0.0f;
        highEnergy = (highCount > 0) ? (highSum / (float)highCount) : 0.0f;
        bandBass8 = (uint8_t)(clamp01(bassEnergy) * 255.0f);

        float avgEnergy = totalEnergy / (float)TUBES;
        bandBassS = smoothAR(bandBassS, bassEnergy, 0.35f, 0.10f);
        bandMidS = smoothAR(bandMidS, midEnergy, 0.25f, 0.08f);
        bandHighS = smoothAR(bandHighS, highEnergy, 0.18f, 0.20f);

        levelFast = levelFast * 0.7f + avgEnergy * 0.3f;
        levelSlow = levelSlow * 0.95f + avgEnergy * 0.05f;

        if (levelFast > levelSlow * beatThreshold && (now - lastBeatTime) > 200) {
            beatDetected = true;
            lastBeatTime = now;
        } else {
            beatDetected = false;
        }

        static uint32_t lastD = 0;
        if (millis() - lastD > 300) {
            Serial.printf(
                "MIC CHECK | Decoded: %ld | dev: %.2f | floor: %.2f | ceil: %.2f | vol: %.4f | gate: %.3f | b/m/h: %.3f %.3f %.3f\n",
                (long)decodeAudioSample(rawSamples[0]),
                avgAbsDev,
                audioFloor,
                audioCeiling,
                volSmooth,
                presenceGate,
                bandBassS,
                bandMidS,
                bandHighS
            );
            lastD = millis();
        }
    }
}
