#ifndef AUDIOFX_H
#define AUDIOFX_H

#include <stdint.h>
#include "hls_stream.h"
#include "ap_axi_sdata.h"

// ─────────────────────────────────────────────
//  Type aliases
// ─────────────────────────────────────────────

typedef int16_t  data_t;      // 16-bit signed audio sample
typedef int32_t  acc_t;       // wider accumulator (pre-gain, mixing)
typedef int32_t  delay_t;     // delay in samples
typedef int16_t  feedback_t;  // Q1.15 feedback coefficient
typedef uint8_t  gain_t;      // pre-gain multiplier (1–16)
typedef uint8_t  crush_t;     // bits to crush (0–14)

// ─────────────────────────────────────────────
//  Effect selector values  (written over AXI-Lite from Jupyter)
// ─────────────────────────────────────────────
#define EFFECT_DISTORTION  0
#define EFFECT_BITCRUSHER  1
#define EFFECT_ECHO        2

// ─────────────────────────────────────────────
//  Echo delay-line size (48 kHz × 1 s headroom)
// ─────────────────────────────────────────────
#define ECHO_BUF_LEN 48000

// ─────────────────────────────────────────────
//  Top-level function declaration
// ─────────────────────────────────────────────
void audiofx(
    hls::stream<ap_axiu<16,0,0,0>> &x_stream,   // AXI-Stream input
    hls::stream<ap_axiu<16,0,0,0>> &y_stream,   // AXI-Stream output

    // ── effect selector ──────────────────────
    uint8_t  effect_select,   // 0=distortion  1=bitcrusher  2=echo

    // ── distortion params ────────────────────
    gain_t   pre_gain,        // 1–16
    data_t   threshold,       // 1–32767

    // ── bitcrusher params ────────────────────
    crush_t  bits_to_crush,   // 0–14

    // ── echo params ──────────────────────────
    delay_t    echo_delay,    // samples (0–47999)
    feedback_t echo_feedback  // Q1.15  (0x0000–0x7FFF)
);

#endif // AUDIOFX_H
