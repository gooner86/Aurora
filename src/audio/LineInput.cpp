#include "LineInput.h"
#include "../Hardware.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include <esp_log.h>

namespace LineInput {
    bool lineOk = false;
    static const char* I2S_TAG = "I2S";
    enum class DecodeMode : uint8_t {
        Shift8,
        High16,
        Low24,
        High24,
    };
    static DecodeMode activeDecode = DecodeMode::Shift8;

    static inline int32_t signExtend24(uint32_t v) {
        if (v & 0x00800000UL) {
            v |= 0xFF000000UL;
        }
        return (int32_t)v;
    }

    static int32_t decodeWithMode(int32_t raw, DecodeMode mode) {
        switch (mode) {
            case DecodeMode::High16:
                return (int32_t)(int16_t)((uint32_t)raw >> 16);
            case DecodeMode::Low24:
                return signExtend24((uint32_t)raw & 0x00FFFFFFUL);
            case DecodeMode::High24:
                return signExtend24((uint32_t)raw >> 8);
            case DecodeMode::Shift8:
            default:
                return raw >> 8;
        }
    }

    int32_t decodeSample(int32_t raw) {
        return decodeWithMode(raw, activeDecode);
    }

    static const char* decodeModeName(DecodeMode mode) {
        switch (mode) {
            case DecodeMode::High16: return "high16";
            case DecodeMode::Low24:  return "low24";
            case DecodeMode::High24: return "high24";
            case DecodeMode::Shift8:
            default:                 return "shift8";
        }
    }

    static float scoreDecode(
        const int32_t* buffer,
        int sampleCount,
        DecodeMode mode,
        float* avgAbsDevOut,
        float* zeroRatioOut,
        float* clipRatioOut
    ) {
        if (sampleCount <= 0) {
            *avgAbsDevOut = 0.0f;
            *zeroRatioOut = 1.0f;
            *clipRatioOut = 1.0f;
            return 0.0f;
        }

        double sum = 0.0;
        int zeroCount = 0;
        int clipCount = 0;
        int transitions = 0;
        int32_t prev = 0;
        int32_t minVal = INT32_MAX;
        int32_t maxVal = INT32_MIN;

        for (int i = 0; i < sampleCount; ++i) {
            int32_t v = decodeWithMode(buffer[i], mode);
            sum += (double)v;
            if (v == 0) zeroCount++;
            if (v == -32768 || v == 32767 || v == -8388608 || v == 8388607) clipCount++;
            if (i > 0 && v != prev) transitions++;
            prev = v;
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }

        float mean = (float)(sum / sampleCount);
        double sumAbsDev = 0.0;
        for (int i = 0; i < sampleCount; ++i) {
            float centered = (float)decodeWithMode(buffer[i], mode) - mean;
            sumAbsDev += fabs(centered);
        }

        float avgAbsDev = (float)(sumAbsDev / sampleCount);
        float zeroRatio = (float)zeroCount / (float)sampleCount;
        float clipRatio = (float)clipCount / (float)sampleCount;
        float transitionRatio = (float)transitions / (float)max(1, sampleCount - 1);
        float range = (float)((int64_t)maxVal - (int64_t)minVal);

        float score = avgAbsDev;
        if (range < 16.0f) score *= 0.01f;
        score *= (0.30f + transitionRatio);
        score *= (1.0f - min(zeroRatio, 0.98f) * 0.70f);
        score *= (1.0f - min(clipRatio, 0.98f) * 0.85f);
        if (avgAbsDev < 2.0f) score = 0.0f;

        *avgAbsDevOut = avgAbsDev;
        *zeroRatioOut = zeroRatio;
        *clipRatioOut = clipRatio;
        return score;
    }

    static unsigned long countPinEdges(int pin, int durationMs = 250) {
        pinMode(pin, INPUT);
        int last = digitalRead(pin);
        unsigned long edges = 0;
        unsigned long end = millis() + durationMs;
        while (millis() < end) {
            int v = digitalRead(pin);
            if (v != last) {
                edges++;
                last = v;
            }
        }
        return edges;
    }

    static void quietDriverUninstall(i2s_port_t port) {
        esp_log_level_t previousLevel = esp_log_level_get(I2S_TAG);
        esp_log_level_set(I2S_TAG, ESP_LOG_NONE);
        i2s_driver_uninstall(port);
        esp_log_level_set(I2S_TAG, previousLevel);
    }

    void init() {
        Serial.println("LineInput: Initializing...");
        lineOk = false;
        activeDecode = DecodeMode::Shift8;
        quietDriverUninstall(I2S_NUM_1);
        quietDriverUninstall(I2S_NUM_0);

        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 44100,
            // The ESP32 DMA path is much more reliable when 24-bit PCM is read
            // into 32-bit slots and unpacked in software.
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 64,
            .use_apll = true, // APLL is better for the PCM1808 clock
            .tx_desc_auto_clear = false,
            .fixed_mclk = 44100 * 256,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT
        };

        i2s_pin_config_t pin_config = {
            .mck_io_num = LINE_SCK,
            .bck_io_num = LINE_BCLK,
            .ws_io_num = LINE_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = LINE_SD
        };

        if (i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL) == ESP_OK) {
            if (i2s_set_pin(I2S_NUM_1, &pin_config) == ESP_OK) {
                delay(20);

                const int probeSamples = 128;
                int32_t probeBuffer[probeSamples];
                size_t bytesRead = 0;
                if (i2s_read(I2S_NUM_1, probeBuffer, sizeof(probeBuffer), &bytesRead, 60) == ESP_OK &&
                    bytesRead >= sizeof(int32_t) * 16) {
                    int sampleCount = (int)(bytesRead / sizeof(int32_t));
                    DecodeMode modes[] = {
                        DecodeMode::Shift8,
                        DecodeMode::High16,
                        DecodeMode::Low24,
                        DecodeMode::High24
                    };

                    float bestScore = 0.0f;
                    float bestAvgAbsDev = 0.0f;
                    float bestZeroRatio = 1.0f;
                    float bestClipRatio = 1.0f;

                    for (DecodeMode mode : modes) {
                        float avgAbsDev = 0.0f;
                        float zeroRatio = 1.0f;
                        float clipRatio = 1.0f;
                        float score = scoreDecode(probeBuffer, sampleCount, mode, &avgAbsDev, &zeroRatio, &clipRatio);
                        if (score > bestScore) {
                            bestScore = score;
                            bestAvgAbsDev = avgAbsDev;
                            bestZeroRatio = zeroRatio;
                            bestClipRatio = clipRatio;
                            activeDecode = mode;
                        }
                    }

                    Serial.printf(
                        "LineInput: decode=%s score=%.2f dev=%.2f zero=%.2f clip=%.2f\n",
                        decodeModeName(activeDecode),
                        bestScore,
                        bestAvgAbsDev,
                        bestZeroRatio,
                        bestClipRatio
                    );

                    if (bestScore <= 0.0f) {
                        unsigned long mclkEdges = countPinEdges(LINE_SCK);
                        unsigned long bclkEdges = countPinEdges(LINE_BCLK);
                        unsigned long wsEdges = countPinEdges(LINE_WS);
                        unsigned long sdEdges = countPinEdges(LINE_SD);
                        Serial.printf(
                            "LineInput: edges mclk=%lu bclk=%lu ws=%lu sd=%lu\n",
                            mclkEdges,
                            bclkEdges,
                            wsEdges,
                            sdEdges
                        );
                    }
                }

                lineOk = true;
            }
        }

        if (lineOk) {
            Serial.println("LineInput: Ready on I2S1.");
        } else {
            Serial.println("LineInput: Init failed.");
        }
    }

    bool readSamples(int32_t* buffer, size_t numSamples) {
        if (!lineOk) return false;
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_1, buffer, numSamples * sizeof(int32_t), &bytesRead, portMAX_DELAY);
        return (bytesRead == (numSamples * sizeof(int32_t)));
    }
}
