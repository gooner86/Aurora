#include "MicInput.h"
#include "../Hardware.h"
#include "driver/i2s.h" // Back to legacy driver for compatibility

namespace MicInput {
    bool micOk = false;

    void init() {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 44100,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
            .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 64,
            .use_apll = false
        };

        i2s_pin_config_t pin_config = {
            .bck_io_num = MIC_BCLK,
            .ws_io_num = MIC_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = MIC_SD
        };

        if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) == ESP_OK) {
            if (i2s_set_pin(I2S_NUM_0, &pin_config) == ESP_OK) {
                micOk = true;
            }
        }
    }

    bool readSamples(int32_t* buffer, size_t numSamples) {
        if (!micOk) return false;
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_0, buffer, numSamples * sizeof(int32_t), &bytesRead, portMAX_DELAY);
        return (bytesRead > 0);
    }
}