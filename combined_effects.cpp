// audiofx.cpp
//
// Single Vitis HLS IP that implements distortion, bitcrusher, and echo.
//
// Interface summary
// ─────────────────
//   AXI-Stream  : x_stream (in), y_stream (out)  — audio data path
//   AXI-Lite    : all other ports                 — control / parameters
//
// effect_select (AXI-Lite, written from Jupyter before each DMA transfer)
//   0 → hard-clip distortion
//   1 → bitcrusher
//   2 → echo
//
// The echo delay-line is a static array so HLS infers on-chip BRAM and
// state (current_index) is preserved across consecutive kernel invocations,
// exactly as in the original echo.cpp.
//
// Algorithm notes
// ───────────────
// Distortion  : y[n] = clip( pre_gain * x[n], -threshold, +threshold )
// Bitcrusher  : y[n] = x[n] & ~((1 << bits_to_crush) - 1)
// Echo        : y[n] = x[n] + feedback * delay_buffer[n - echo_delay]
//               delay_buffer[n] = x[n]     (stores dry signal)
//               feedback is Q1.15 fixed-point

#include "audiofx.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Persistent echo state (BRAM-inferred by HLS)
// ─────────────────────────────────────────────────────────────────────────────
static data_t delay_buffer[ECHO_BUF_LEN];
static int    current_index = 0;

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level function
// ─────────────────────────────────────────────────────────────────────────────
void audiofx(
    hls::stream<ap_axiu<16,0,0,0>> &x_stream,
    hls::stream<ap_axiu<16,0,0,0>> &y_stream,

    uint8_t    effect_select,
    gain_t     pre_gain,
    data_t     threshold,
    crush_t    bits_to_crush,
    delay_t    echo_delay,
    feedback_t echo_feedback
)
{
// ── AXI-Stream ports ─────────────────────────────────────────────────────────
#pragma HLS INTERFACE axis      port=x_stream
#pragma HLS INTERFACE axis      port=y_stream

// ── AXI-Lite parameter ports ─────────────────────────────────────────────────
#pragma HLS INTERFACE s_axilite port=effect_select
#pragma HLS INTERFACE s_axilite port=pre_gain
#pragma HLS INTERFACE s_axilite port=threshold
#pragma HLS INTERFACE s_axilite port=bits_to_crush
#pragma HLS INTERFACE s_axilite port=echo_delay
#pragma HLS INTERFACE s_axilite port=echo_feedback
#pragma HLS INTERFACE s_axilite port=return   // ap_ctrl_hs block-level control

// ── Keep delay buffer in BRAM, not distributed RAM ───────────────────────────
#pragma HLS BIND_STORAGE variable=delay_buffer type=RAM_2P impl=BRAM

    // ── Pre-compute bitcrusher mask once per invocation ──────────────────────
    // This is a scalar, computed outside the sample loop so HLS does not
    // re-evaluate it every cycle.
    uint16_t bc_mask = (bits_to_crush == 0)
                       ? (uint16_t)0xFFFF
                       : (uint16_t)(~((1u << bits_to_crush) - 1u));

    // ── Clamp echo_delay to valid range ──────────────────────────────────────
    delay_t safe_delay = echo_delay;
    if (safe_delay < 0)            safe_delay = 0;
    if (safe_delay >= ECHO_BUF_LEN) safe_delay = ECHO_BUF_LEN - 1;

    // ── Sample-processing loop ───────────────────────────────────────────────
    // Runs until the upstream DMA asserts TLAST on the final sample.
    ap_axiu<16,0,0,0> in_samp, out_samp;

    do {
#pragma HLS PIPELINE II=1

        x_stream.read(in_samp);
        data_t x = (data_t)(int16_t)in_samp.data;   // sign-extend from 16-bit AXI slot
        data_t y = 0;

        // ── Effect routing ───────────────────────────────────────────────────
        if (effect_select == EFFECT_DISTORTION) {
            // Hard-clip distortion
            // y[n] = clip( pre_gain * x[n], -threshold, +threshold )
            acc_t amplified = (acc_t)x * (acc_t)pre_gain;

            if      (amplified >  (acc_t)threshold)  y =  threshold;
            else if (amplified < -(acc_t)threshold)  y = -threshold;
            else                                     y =  (data_t)amplified;

        } else if (effect_select == EFFECT_BITCRUSHER) {
            // Bitcrusher — zero the lowest bits_to_crush bits
            // y[n] = x[n] & ~((1 << bits_to_crush) - 1)
            //
            // Cast through uint16_t so the AND operates on the raw bit pattern
            // without sign-extension artefacts, then reinterpret as signed.
            y = (data_t)((uint16_t)(int16_t)x & bc_mask);

        } else {
            // Echo (effect_select == EFFECT_ECHO, default)
            // y[n] = x[n] + feedback * delay_buffer[n - echo_delay]
            // delay_buffer[n] = x[n]   (dry signal written after mixing)

            int echo_idx = current_index - (int)safe_delay;
            if (echo_idx < 0) echo_idx += ECHO_BUF_LEN;

            data_t  delayed = delay_buffer[echo_idx];

            // feedback is Q1.15: multiply then arithmetic-shift right 15
            int32_t mixed = (int32_t)x
                          + ((int32_t)delayed * (int32_t)echo_feedback >> 15);

            // Saturate to 16-bit range
            if (mixed >  32767) mixed =  32767;
            if (mixed < -32768) mixed = -32768;

            // Write dry input into delay buffer, advance ring-buffer head
            delay_buffer[current_index] = x;
            current_index = (current_index + 1 >= ECHO_BUF_LEN)
                            ? 0
                            : current_index + 1;

            y = (data_t)mixed;
        }

        // ── Pack output sample and forward downstream ─────────────────────
        out_samp.data = (ap_uint<16>)(int16_t)y;
        out_samp.last = in_samp.last;   // propagate TLAST
        out_samp.keep = in_samp.keep;
        y_stream.write(out_samp);

    } while (!in_samp.last);
}
