#include "LineInput.h"
#include "../Hardware.h"
#include "driver/i2s.h"

namespace LineInput {
    bool lineOk = false;

    void init() {
        i2s_config_t i2s_config = {
            .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
            .sample_rate = 44100,
            .bits_per_sample = I2S_BITS_PER_SAMPLE_24BIT, // Matches PCM1808
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
            .dma_buf_count = 8,
            .dma_buf_len = 64,
            .use_apll = true // APLL is better for the PCM1808 clock
        };

        i2s_pin_config_t pin_config = {
            .bck_io_num = LINE_BCLK,
            .ws_io_num = LINE_WS,
            .data_out_num = I2S_PIN_NO_CHANGE,
            .data_in_num = LINE_SD
        };

        if (i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL) == ESP_OK) {
            // This is how we output the Master Clock (MCLK) on GPIO 0 for the legacy driver
            PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
            WRITE_PERI_REG(PIN_CTRL, 0xFFF0); 
            
            if (i2s_set_pin(I2S_NUM_1, &pin_config) == ESP_OK) {
                lineOk = true;
            }
        }
    }

    bool readSamples(int32_t* buffer, size_t numSamples) {
        if (!lineOk) return false;
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_1, buffer, numSamples * sizeof(int32_t), &bytesRead, portMAX_DELAY);
        return (bytesRead > 0);
    }
}
