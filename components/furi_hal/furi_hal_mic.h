/**
 * @file furi_hal_mic.h
 * PDM microphone capture (16-bit signed mono PCM) for boards with a mic. On the
 * T-Embed CC1101 the mic is a PDM microphone on I2S (data/clk pins from the
 * board header). The speaker permanently owns I2S0, so the mic uses I2S1.
 *
 * NOTE: whether ESP32-S3 PDM RX is available on I2S1 must be confirmed on
 * hardware; furi_hal_mic_start returns false (rather than crashing) if the I2S
 * PDM RX channel cannot be brought up, so callers degrade gracefully.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Start capture at sample_rate Hz (0 = 16000). Returns false if unavailable. */
bool furi_hal_mic_start(uint32_t sample_rate);

/** Read up to max_samples 16-bit samples, blocking up to timeout_ms. Returns
 * the number of samples read (0 on timeout or if not started). */
size_t furi_hal_mic_read(int16_t* samples, size_t max_samples, uint32_t timeout_ms);

/** Stop capture and release the I2S channel. */
void furi_hal_mic_stop(void);

/** @return true while capture is active. */
bool furi_hal_mic_is_active(void);

#ifdef __cplusplus
}
#endif
