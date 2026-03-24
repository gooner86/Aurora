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
static float splitReference[TUBES];
static float bandWeight[TUBES];
static uint16_t tempoHistory[6];
static uint8_t tempoHistoryCount = 0;
static uint8_t tempoHistoryIndex = 0;

static inline float smoothAR(float cur, float target, float attack, float release) {
    float k = (target > cur) ? attack : release;
    return cur + (target - cur) * k;
}

static inline void trackNoiseWindow(
    float sample,
    float &floor,
    float &ceiling,
    float minSpan,
    float floorRise,
    float floorFall,
    float ceilingAttack,
    float ceilingRelease
) {
    if (floor <= 0.0f) {
        floor = sample;
    }
    if (ceiling <= 0.0f) {
        ceiling = floor + minSpan;
    }

    float floorBlend = (sample > floor) ? floorRise : floorFall;
    floor = lerpf(floor, sample, floorBlend);

    float ceilingTarget = max(sample, floor + minSpan);
    float ceilingBlend = (ceilingTarget > ceiling) ? ceilingAttack : ceilingRelease;
    ceiling = lerpf(ceiling, ceilingTarget, ceilingBlend);

    if (ceiling < floor + minSpan) {
        ceiling = floor + minSpan;
    }
}

static inline void trackSplitReference(
    float desired,
    float baseline,
    float &reference,
    float riseBlend,
    float fallBlend
) {
    float target = max(desired, baseline);
    if (reference <= 0.0f) {
        reference = target;
        return;
    }

    float blend = (target > reference) ? riseBlend : fallBlend;
    reference = lerpf(reference, target, blend);
    if (reference < baseline) {
        reference = baseline;
    }
}

static inline int32_t decodeAudioSample(int32_t raw) {
    if (currentAudio == AudioSource::MIC_IN) {
        return MicInput::decodeSample(raw);
    }
    if (currentAudio == AudioSource::LINE_IN) {
        return LineInput::decodeSample(raw);
    }
    return 0;
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

static void clearTempoTracking() {
    for (int i = 0; i < 6; ++i) {
        tempoHistory[i] = 0;
    }
    tempoHistoryCount = 0;
    tempoHistoryIndex = 0;
    beatPulse = 0.0f;
    tempoBPM = 0.0f;
    tempoConfidence = 0.0f;
    tempoNormalized = 0.0f;
    beatIntervalMs = 0;
}

static float medianTempoInterval() {
    if (tempoHistoryCount == 0) {
        return 0.0f;
    }

    uint16_t sorted[6];
    for (uint8_t i = 0; i < tempoHistoryCount; ++i) {
        sorted[i] = tempoHistory[i];
    }

    for (uint8_t i = 1; i < tempoHistoryCount; ++i) {
        uint16_t key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            --j;
        }
        sorted[j + 1] = key;
    }

    if ((tempoHistoryCount & 1U) == 1U) {
        return (float)sorted[tempoHistoryCount / 2];
    }

    uint8_t hi = tempoHistoryCount / 2;
    uint8_t lo = hi - 1;
    return ((float)sorted[lo] + (float)sorted[hi]) * 0.5f;
}

static void pushTempoInterval(uint32_t intervalMs) {
    if (intervalMs < 240 || intervalMs > 1500) {
        return;
    }

    tempoHistory[tempoHistoryIndex] = (uint16_t)intervalMs;
    tempoHistoryIndex = (uint8_t)((tempoHistoryIndex + 1) % 6);
    if (tempoHistoryCount < 6) {
        tempoHistoryCount++;
    }

    float medianInterval = medianTempoInterval();
    if (medianInterval <= 0.0f) {
        return;
    }

    beatIntervalMs = (uint32_t)lroundf(medianInterval);
    float bpm = 60000.0f / medianInterval;
    if (tempoBPM <= 0.0f) {
        tempoBPM = bpm;
    } else {
        tempoBPM = lerpf(tempoBPM, bpm, 0.28f);
    }

    tempoNormalized = clamp01((tempoBPM - 70.0f) / 90.0f);
    tempoConfidence = clamp01(tempoConfidence + 0.18f);
}

static void decayToSilence() {
    for (int t = 0; t < TUBES; ++t) {
        tubeBandsInstant[t] = 0.0f;
        tubeBandsSplitInstant[t] = 0.0f;
        tubeBandsSmooth[t] = smoothAR(tubeBandsSmooth[t], 0.0f, 0.10f, 0.18f);
        tubeBandsSplitSmooth[t] = smoothAR(tubeBandsSplitSmooth[t], 0.0f, 0.16f, 0.22f);
        splitReference[t] = lerpf(splitReference[t], 0.0f, 0.03f);
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
    beatPulse = lerpf(beatPulse, 0.0f, 0.18f);
    tempoConfidence = lerpf(tempoConfidence, 0.0f, 0.05f);
    if (tempoConfidence < 0.05f) {
        clearTempoTracking();
    }
}

namespace AudioAnalyzer {
    void init() {
        for (int i = 0; i < TUBES; ++i) {
            tubeMax[i] = 0.0f;
            bandFloor[i] = 0.0f;
            bandCeiling[i] = 0.0f;
            splitReference[i] = 0.0f;
        }
        audioFloor = 20000.0f;
        audioCeiling = 180000.0f;
        clearTempoTracking();
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
        double rawAbsSum = 0.0;
        for (int i = 0; i < FFT_SAMPLES; ++i) {
            int32_t sample = decodeAudioSample(rawSamples[i]);
            sum += (double)sample;
            rawAbsSum += fabs((double)sample);
        }
        float mean = (float)(sum / FFT_SAMPLES);

        for (int i = 0; i < FFT_SAMPLES; ++i) {
            float centered = (float)decodeAudioSample(rawSamples[i]) - mean;
            vReal[i] = (double)centered;
            vImag[i] = 0.0;
        }

        float avgAbs = (float)(rawAbsSum / FFT_SAMPLES);

        FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
        FFT.compute(FFTDirection::Forward);
        FFT.complexToMagnitude();
        vReal[0] = 0.0;
        vReal[1] = 0.0;
        beatPulse = lerpf(beatPulse, 0.0f, 0.14f);
        float sens = (MASTER_SENSITIVITY * MASTER_SENSITIVITY);
        trackNoiseWindow(avgAbs, audioFloor, audioCeiling, 40000.0f, 0.010f, 0.0025f, 0.22f, 0.010f);
        float audioExcursion = max(0.0f, avgAbs - (audioFloor + 3000.0f));
        float audioRange = max(40000.0f, audioCeiling - audioFloor);
        float inputLevel = clamp01((audioExcursion / (audioRange * 0.55f)) * sens);
        float presenceGate = clamp01((audioExcursion / (audioRange * 0.70f)) * 1.35f);
        presenceGate = powf(presenceGate, 1.35f);
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

            float bandPos = (TUBES > 1) ? ((float)t / (float)(TUBES - 1)) : 0.0f;
            float lowBandBlend = clamp01(bandPos * 4.0f);
            float rumbleBias = lerpf(1.75f, 1.0f, lowBandBlend);
            float bandLevel = max((float)bandSum, 0.0f) * bandWeight[t];
            float minBandSpan = lerpf(22000.0f, 9000.0f, bandPos);

            trackNoiseWindow(bandLevel, bandFloor[t], bandCeiling[t], minBandSpan, 0.012f, 0.004f, 0.24f, 0.012f);
            float bandExcursion = max(0.0f, bandLevel - (bandFloor[t] + (minBandSpan * 0.08f * rumbleBias)));
            float bandRange = max(minBandSpan, bandCeiling[t] - bandFloor[t]);
            float splitBaseline = minBandSpan * lerpf(0.98f, 0.66f, bandPos);
            float splitDesired = bandRange * lerpf(0.78f, 0.52f, bandPos);
            float splitRise = lerpf(0.018f, 0.045f, bandPos);
            float splitFall = lerpf(0.006f, 0.016f, bandPos);
            trackSplitReference(splitDesired, splitBaseline, splitReference[t], splitRise, splitFall);

            float splitNormalized = clamp01((bandExcursion / (splitReference[t] * 0.60f)) * sens);
            splitNormalized = powf(splitNormalized, lerpf(1.28f, 1.02f, clamp01(MASTER_SENSITIVITY / 3.0f)));
            float lowBandGate = lerpf(clamp01(presenceGate * 1.35f), 1.0f, lowBandBlend);
            splitNormalized *= lowBandGate;
            float bandNormalized = splitNormalized * presenceGate;
            bandNormalized = powf(bandNormalized, lerpf(1.45f, 1.10f, clamp01(MASTER_SENSITIVITY / 3.0f)));

            tubeMax[t] = bandCeiling[t];
            tubeBandsSplitInstant[t] = splitNormalized;
            tubeBandsInstant[t] = bandNormalized;
            tubeBandsSplitSmooth[t] = smoothAR(tubeBandsSplitSmooth[t], splitNormalized, 0.56f, 0.24f);
            tubeBandsSmooth[t] = smoothAR(tubeBandsSmooth[t], bandNormalized, 0.35f, 0.12f);
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

        uint32_t sinceLastBeat = (lastBeatTime > 0) ? (now - lastBeatTime) : 1000;
        if (levelFast > levelSlow * beatThreshold && sinceLastBeat > 200) {
            beatDetected = true;
            beatPulse = 1.0f;
            if (sinceLastBeat > 0) {
                pushTempoInterval(sinceLastBeat);
            }
            lastBeatTime = now;
        } else {
            beatDetected = false;
        }

        if (tempoConfidence > 0.0f) {
            float decay = 0.0035f;
            if (!tempoReady()) {
                decay = 0.012f;
            }
            if (beatIntervalMs > 0 && lastBeatTime > 0 && (now - lastBeatTime) > (beatIntervalMs * 4U)) {
                decay = 0.045f;
            }
            tempoConfidence = lerpf(tempoConfidence, 0.0f, decay);
            if (tempoConfidence < 0.06f) {
                clearTempoTracking();
            } else {
                tempoNormalized = clamp01((tempoBPM - 70.0f) / 90.0f);
            }
        }

    }
}
