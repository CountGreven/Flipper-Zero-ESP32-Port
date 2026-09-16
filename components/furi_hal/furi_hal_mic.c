#include "furi_hal_mic.h"

#include "sdkconfig.h"

#if(CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32S2)

#include "boards/board.h"
#include <furi.h>

#if defined(BOARD_HAS_MIC) && BOARD_HAS_MIC

#include <driver/i2s_pdm.h>

#define TAG "FuriHalMic"

/* I2S0 is held by the speaker from boot, so the mic runs on I2S1. */
#define MIC_I2S_PORT I2S_NUM_1

static i2s_chan_handle_t s_rx = NULL;
static bool s_active = false;

bool furi_hal_mic_start(uint32_t sample_rate) {
    if(s_active) return true;
    if(sample_rate == 0) sample_rate = 16000;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(MIC_I2S_PORT, I2S_ROLE_MASTER);
    if(i2s_new_channel(&chan_cfg, NULL, &s_rx) != ESP_OK) {
        FURI_LOG_E(TAG, "i2s_new_channel failed");
        s_rx = NULL;
        return false;
    }

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg =
            I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .clk = (gpio_num_t)BOARD_PIN_MIC_CLK,
                .din = (gpio_num_t)BOARD_PIN_MIC_DATA,
                .invert_flags = {.clk_inv = false},
            },
    };
    if(i2s_channel_init_pdm_rx_mode(s_rx, &pdm_cfg) != ESP_OK) {
        FURI_LOG_E(TAG, "PDM RX init failed (port may not support it)");
        i2s_del_channel(s_rx);
        s_rx = NULL;
        return false;
    }
    if(i2s_channel_enable(s_rx) != ESP_OK) {
        i2s_del_channel(s_rx);
        s_rx = NULL;
        return false;
    }
    s_active = true;
    FURI_LOG_I(TAG, "mic started @ %lu Hz", (unsigned long)sample_rate);
    return true;
}

size_t furi_hal_mic_read(int16_t* samples, size_t max_samples, uint32_t timeout_ms) {
    if(!s_active || !samples || !max_samples) return 0;
    size_t read_bytes = 0;
    if(i2s_channel_read(s_rx, samples, max_samples * sizeof(int16_t), &read_bytes, timeout_ms) !=
       ESP_OK)
        return 0;
    return read_bytes / sizeof(int16_t);
}

void furi_hal_mic_stop(void) {
    if(!s_active) return;
    i2s_channel_disable(s_rx);
    i2s_del_channel(s_rx);
    s_rx = NULL;
    s_active = false;
}

bool furi_hal_mic_is_active(void) {
    return s_active;
}

#else /* board has no mic */

bool furi_hal_mic_start(uint32_t sr) {
    (void)sr;
    return false;
}
size_t furi_hal_mic_read(int16_t* s, size_t n, uint32_t t) {
    (void)s;
    (void)n;
    (void)t;
    return 0;
}
void furi_hal_mic_stop(void) {
}
bool furi_hal_mic_is_active(void) {
    return false;
}

#endif

#else /* not S3/S2 */

bool furi_hal_mic_start(uint32_t sr) {
    (void)sr;
    return false;
}
size_t furi_hal_mic_read(int16_t* s, size_t n, uint32_t t) {
    (void)s;
    (void)n;
    (void)t;
    return 0;
}
void furi_hal_mic_stop(void) {
}
bool furi_hal_mic_is_active(void) {
    return false;
}

#endif
