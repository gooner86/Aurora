#include "MicInput.h"
#include "../Hardware.h"
#include "driver/i2s.h"
#include <esp_log.h>

namespace MicInput {
    bool micOk = false;
    static i2s_port_t activePort = I2S_NUM_1;
    static const char* I2S_TAG = "I2S";

    enum class DecodeMode : uint8_t {
        Shift14,
        Shift8,
        High16,
        Low24,
        High24,
    };

    struct ProbeResult {
        bool valid = false;
        float score = 0.0f;
        i2s_port_t port = I2S_NUM_1;
        uint32_t sampleRate = 44100;
        i2s_bits_per_sample_t bits = I2S_BITS_PER_SAMPLE_32BIT;
        i2s_channel_fmt_t channel = I2S_CHANNEL_FMT_ONLY_LEFT;
        DecodeMode decode = DecodeMode::Shift14;
        float avgAbsDev = 0.0f;
        float zeroRatio = 1.0f;
        float clipRatio = 1.0f;
    };

    static DecodeMode activeDecode = DecodeMode::Shift14;

    static inline int32_t signExtend24(uint32_t v) {
        if (v & 0x00800000UL) {
            v |= 0xFF000000UL;
        }
        return (int32_t)v;
    }

    static int32_t decodeWithMode(int32_t raw, DecodeMode mode) {
        switch (mode) {
            case DecodeMode::Shift8:
                return raw >> 8;
            case DecodeMode::High16:
                return (int32_t)(int16_t)((uint32_t)raw >> 16);
            case DecodeMode::Low24:
                return signExtend24((uint32_t)raw & 0x00FFFFFFUL);
            case DecodeMode::High24:
                return signExtend24((uint32_t)raw >> 8);
            case DecodeMode::Shift14:
            default:
                return raw >> 14;
        }
    }

    int32_t decodeSample(int32_t raw) {
        return decodeWithMode(raw, activeDecode);
    }

    static const char* decodeModeName(DecodeMode mode) {
        switch (mode) {
            case DecodeMode::Shift8:
                return "shift8";
            case DecodeMode::High16:
                return "high16";
            case DecodeMode::Low24:
                return "low24";
            case DecodeMode::High24:
                return "high24";
            case DecodeMode::Shift14:
            default:
                return "shift14";
        }
    }

    static void quietDriverUninstall(i2s_port_t port) {
        esp_log_level_t previousLevel = esp_log_level_get(I2S_TAG);
        esp_log_level_set(I2S_TAG, ESP_LOG_NONE);
        i2s_driver_uninstall(port);
        esp_log_level_set(I2S_TAG, previousLevel);
    }

    static void sd_line_monitor(int gpioNum, int durationMs = 2000) {
        pinMode(gpioNum, INPUT);
        int last = digitalRead(gpioNum);
        unsigned long edges = 0;
        unsigned long end = millis() + durationMs;
        while (millis() < end) {
            int v = digitalRead(gpioNum);
            if (v != last) {
                edges++;
                last = v;
            }
        }
        Serial.printf("I2S SD monitor: pin=%d edges=%u over %dms\n", gpioNum, (unsigned)edges, durationMs);
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
            if (v == 0) {
                zeroCount++;
            }
            if (v == -32768 || v == 32767 || v == -8388608 || v == 8388607) {
                clipCount++;
            }
            if (i > 0 && v != prev) {
                transitions++;
            }
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
        if (range < 16.0f) {
            score *= 0.01f;
        }
        score *= (0.25f + transitionRatio);
        score *= (1.0f - min(zeroRatio, 0.98f) * 0.70f);
        score *= (1.0f - min(clipRatio, 0.98f) * 0.85f);
        if (avgAbsDev < 2.0f) {
            score = 0.0f;
        }

        *avgAbsDevOut = avgAbsDev;
        *zeroRatioOut = zeroRatio;
        *clipRatioOut = clipRatio;
        return score;
    }

    static bool probeConfig(
        i2s_port_t port,
        uint32_t sampleRate,
        i2s_bits_per_sample_t bits,
        i2s_channel_fmt_t channel,
        const i2s_pin_config_t& pinConfig,
        ProbeResult* best
    ) {
        i2s_config_t cfg = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = sampleRate,
            .bits_per_sample = bits,
            .channel_format = channel,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 64,
            .use_apll = false
        };

        quietDriverUninstall(port);
        if (i2s_driver_install(port, &cfg, 0, NULL) != ESP_OK) {
            return false;
        }
        if (i2s_set_pin(port, &pinConfig) != ESP_OK) {
            quietDriverUninstall(port);
            return false;
        }

        delay(20);

        const int probeSamples = 128;
        int32_t probeBuffer[probeSamples];
        size_t bytesRead = 0;
        if (i2s_read(port, probeBuffer, sizeof(probeBuffer), &bytesRead, 60) != ESP_OK || bytesRead < sizeof(int32_t) * 16) {
            quietDriverUninstall(port);
            return false;
        }

        int sampleCount = (int)(bytesRead / sizeof(int32_t));
        DecodeMode decodeModes[] = {
            DecodeMode::Shift14,
            DecodeMode::Shift8,
            DecodeMode::High16,
            DecodeMode::Low24,
            DecodeMode::High24
        };

        ProbeResult localBest;
        localBest.valid = false;

        for (DecodeMode mode : decodeModes) {
            float avgAbsDev = 0.0f;
            float zeroRatio = 1.0f;
            float clipRatio = 1.0f;
            float score = scoreDecode(probeBuffer, sampleCount, mode, &avgAbsDev, &zeroRatio, &clipRatio);

            if (!localBest.valid || score > localBest.score) {
                localBest.valid = true;
                localBest.score = score;
                localBest.port = port;
                localBest.sampleRate = sampleRate;
                localBest.bits = bits;
                localBest.channel = channel;
                localBest.decode = mode;
                localBest.avgAbsDev = avgAbsDev;
                localBest.zeroRatio = zeroRatio;
                localBest.clipRatio = clipRatio;
            }
        }

        if (localBest.valid && localBest.score > best->score) {
            *best = localBest;
            best->valid = true;
        }

        quietDriverUninstall(port);
        return localBest.valid;
    }

    void init() {
        Serial.println("Mic: Probing I2S configurations...");
        micOk = false;
        activePort = I2S_NUM_1;
        activeDecode = DecodeMode::Shift14;

        quietDriverUninstall(I2S_NUM_1);
        quietDriverUninstall(I2S_NUM_0);

        pinMode(MIC_BCLK, INPUT);
        pinMode(MIC_WS, INPUT);
        pinMode(MIC_SD, INPUT);
        delay(100);

        i2s_pin_config_t pin_config = {
            .bck_io_num = MIC_BCLK,
            .ws_io_num = MIC_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = MIC_SD
        };

        ProbeResult best;
        i2s_port_t ports[] = { I2S_NUM_1, I2S_NUM_0 };
        uint32_t sampleRates[] = { 44100, 48000, 32000, 16000 };
        i2s_bits_per_sample_t bitsOptions[] = {
            I2S_BITS_PER_SAMPLE_32BIT,
            I2S_BITS_PER_SAMPLE_24BIT,
            I2S_BITS_PER_SAMPLE_16BIT
        };
        i2s_channel_fmt_t channelOptions[] = {
            I2S_CHANNEL_FMT_ONLY_LEFT,
            I2S_CHANNEL_FMT_ONLY_RIGHT,
            I2S_CHANNEL_FMT_RIGHT_LEFT
        };

        for (i2s_port_t port : ports) {
            for (uint32_t sampleRate : sampleRates) {
                for (i2s_bits_per_sample_t bits : bitsOptions) {
                    for (i2s_channel_fmt_t channel : channelOptions) {
                        probeConfig(port, sampleRate, bits, channel, pin_config, &best);
                    }
                }
            }
        }

        if (best.valid) {
            activePort = best.port;
            activeDecode = best.decode;

            i2s_config_t finalCfg = {
                .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                .sample_rate = best.sampleRate,
                .bits_per_sample = best.bits,
                .channel_format = best.channel,
                .communication_format = I2S_COMM_FORMAT_STAND_I2S,
                .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                .dma_buf_count = 8,
                .dma_buf_len = 64,
                .use_apll = false
            };

            if (i2s_driver_install(activePort, &finalCfg, 0, NULL) == ESP_OK &&
                i2s_set_pin(activePort, &pin_config) == ESP_OK) {
                micOk = true;
                Serial.printf(
                    "I2S: Selected port=%d sr=%u bits=%d ch=%d decode=%s score=%.2f dev=%.2f zero=%.2f clip=%.2f\n",
                    (int)activePort,
                    best.sampleRate,
                    (int)best.bits,
                    (int)best.channel,
                    decodeModeName(best.decode),
                    best.score,
                    best.avgAbsDev,
                    best.zeroRatio,
                    best.clipRatio
                );
                return;
            }
            quietDriverUninstall(activePort);
        }

        sd_line_monitor(pin_config.data_in_num, 2000);
        sd_line_monitor(pin_config.ws_io_num, 2000);
        Serial.println("I2S: No usable mic config found.");
    }

    bool readSamples(int32_t* buffer, size_t numSamples) {
        if (!micOk) return false;
        size_t bytesRead = 0;
        i2s_read(activePort, buffer, numSamples * sizeof(int32_t), &bytesRead, 10);
        return (bytesRead > 0);
    }
}
