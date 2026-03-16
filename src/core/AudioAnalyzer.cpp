#include "AudioAnalyzer.h"
#include "../audio/LineInput.h"
#include "../audio/MicInput.h"
#include "../State.h"
#include <math.h>

#define FFT_SAMPLES 1024

static int32_t rawSamples[FFT_SAMPLES];
static uint32_t lastAudioMs = 0;
static float audioFloor = 20000.0f;
static float audioCeiling = 180000.0f;

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

static void decayToSilence() {
    for (int t = 0; t < TUBES; ++t) {
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
            tubeMax[i] = 1.0f;
        }
        audioFloor = 20000.0f;
        audioCeiling = 180000.0f;
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
            sumAbsDev += fabs(centered);
        }

        float avgAbsDev = (float)(sumAbsDev / FFT_SAMPLES);

        if (avgAbsDev < audioFloor) {
            audioFloor = (audioFloor * 0.96f) + (avgAbsDev * 0.04f);
        } else {
            audioFloor = (audioFloor * 0.999f) + (avgAbsDev * 0.001f);
        }

        if (avgAbsDev > audioCeiling) {
            audioCeiling = (audioCeiling * 0.88f) + (avgAbsDev * 0.12f);
        } else {
            audioCeiling = (audioCeiling * 0.9995f) + (avgAbsDev * 0.0005f);
        }

        if (audioFloor < 2000.0f) {
            audioFloor = 2000.0f;
        }
        if (audioCeiling < audioFloor * 1.8f) {
            audioCeiling = audioFloor * 1.8f;
        }

        float gated = avgAbsDev - (audioFloor * 1.10f);
        if (gated < 0.0f) {
            gated = 0.0f;
        }

        float dynamicRange = audioCeiling - audioFloor;
        if (dynamicRange < 5000.0f) {
            dynamicRange = 5000.0f;
        }

        float normalized = clamp01((gated / dynamicRange) * (MASTER_SENSITIVITY * 0.85f));
        float inputLevel = sqrtf(normalized);

        volSmooth = smoothAR(volSmooth, inputLevel, 0.18f, 0.06f);
        volBeat = smoothAR(volBeat, inputLevel, 0.45f, 0.12f);

        float totalEnergy = 0.0f;
        for (int t = 0; t < TUBES; ++t) {
            tubeBandsSmooth[t] = smoothAR(tubeBandsSmooth[t], volSmooth, 0.35f, 0.12f);
            totalEnergy += tubeBandsSmooth[t];
        }

        float avgEnergy = totalEnergy / (float)TUBES;
        bandBassS = smoothAR(bandBassS, avgEnergy, 0.35f, 0.10f);
        bandMidS = smoothAR(bandMidS, avgEnergy, 0.25f, 0.08f);
        bandHighS = smoothAR(bandHighS, avgEnergy, 0.18f, 0.20f);

        bassEnergy = avgEnergy;
        midEnergy = avgEnergy;
        highEnergy = avgEnergy;

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
                "MIC CHECK | Decoded: %ld | dev: %.2f | floor: %.2f | ceil: %.2f | vol: %.4f\n",
                (long)decodeAudioSample(rawSamples[0]),
                avgAbsDev,
                audioFloor,
                audioCeiling,
                volSmooth,
                bassEnergy
            );
            lastD = millis();
        }
    }
}
