#include "MicInput.h"
#include "../Hardware.h"
#include "driver/i2s.h" 

namespace MicInput {
    bool micOk = false;

    void init() {
        Serial.println("Mic: Hard-Resetting I2S Pins...");
        
        // 1. Fully uninstall to release any "ghost" pin ownership
        i2s_driver_uninstall(I2S_NUM_1);
        i2s_driver_uninstall(I2S_NUM_0);
        
        // 2. Force the pins to a neutral state before I2S takes them
        pinMode(25, INPUT);
        pinMode(26, INPUT);
        pinMode(32, INPUT);
        delay(100);

        // Pin mapping for the mic
        i2s_pin_config_t pin_config = {
            .bck_io_num = 26,
            .ws_io_num = 25,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = 32
        };

        // Auto-detect: try common bit depths and channel formats
        i2s_bits_per_sample_t bitsOptions[] = { I2S_BITS_PER_SAMPLE_32BIT, I2S_BITS_PER_SAMPLE_24BIT, I2S_BITS_PER_SAMPLE_16BIT };
        i2s_channel_fmt_t chOptions[] = { I2S_CHANNEL_FMT_ONLY_LEFT, I2S_CHANNEL_FMT_RIGHT_LEFT };

        const int dbgSamples = 32;
        int32_t dbgBuf[dbgSamples];

        for (size_t bi = 0; bi < sizeof(bitsOptions)/sizeof(bitsOptions[0]) && !micOk; ++bi) {
            for (size_t ci = 0; ci < sizeof(chOptions)/sizeof(chOptions[0]) && !micOk; ++ci) {
                i2s_config_t cfg = {
                    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
                    .sample_rate = 44100,
                    .bits_per_sample = bitsOptions[bi],
                    .channel_format = chOptions[ci],
                    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
                    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                    .dma_buf_count = 8,
                    .dma_buf_len = 64,
                    .use_apll = false
                };

                // ensure previous driver removed
                i2s_driver_uninstall(I2S_NUM_1);

                esp_err_t installRes = i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
                Serial.printf("I2S: trying bits=%d ch=%d install=%d\n", (int)bitsOptions[bi], (int)chOptions[ci], (int)installRes);
                if (installRes != ESP_OK) continue;

                if (i2s_set_pin(I2S_NUM_1, &pin_config) != ESP_OK) {
                    Serial.println("I2S: set_pin failed for this config");
                    i2s_driver_uninstall(I2S_NUM_1);
                    continue;
                }

                size_t bytesRead = 0;
                esp_err_t r = i2s_read(I2S_NUM_1, dbgBuf, dbgSamples * sizeof(int32_t), &bytesRead, 100);
                Serial.printf("I2S: dbg read res=%d bytes=%u\n", (int)r, (unsigned)bytesRead);

                int toPrint = (int)min((size_t)dbgSamples, bytesRead / sizeof(int32_t));
                bool anyNonZero = false;
                Serial.print("I2S DBG SAMPLES: ");
                for (int i = 0; i < toPrint; i++) {
                    Serial.printf("0x%08X ", (uint32_t)dbgBuf[i]);
                    if (dbgBuf[i] != 0) anyNonZero = true;
                }
                Serial.println();

                if (anyNonZero) {
                    micOk = true;
                    Serial.printf("I2S: Detected working config bits=%d ch=%d\n", (int)bitsOptions[bi], (int)chOptions[ci]);
                    break;
                }

                i2s_driver_uninstall(I2S_NUM_1);
            }
        }

        if (micOk) {
            Serial.println("I2S: Clocking Active on Pins 25/26.");
        } else {
            Serial.println("I2S: No data detected on MIC pins after probing.");
        }
    }

    bool readSamples(int32_t* buffer, size_t numSamples) {
        if (!micOk) return false;
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_1, buffer, numSamples * sizeof(int32_t), &bytesRead, 10); 
        return (bytesRead > 0);
    }
}