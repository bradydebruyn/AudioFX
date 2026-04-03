// echo.cpp  –  AXI-Stream version
// Parameters (delay, feedback) remain AXI-Lite side-channel registers.
// Audio data flows through an AXI-Stream pair so the full block is
// transferred in one DMA burst instead of one round-trip per sample.
//
// delay_buffer / current_index stay static and HLS infers on-chip BRAM,
// state is preserved across consecutive kernel invocations.

#include "echo.h"

static data_t delay_buffer[48000];
static int    current_index = 0;

void echo(
    hls::stream<ap_axiu<16,0,0,0>> &x_stream,  // input  AXI-Stream
    hls::stream<ap_axiu<16,0,0,0>> &y_stream,  // output AXI-Stream
    delay_t    delay,                           // AXI-Lite: num samples to delay
    feedback_t feedback                         // AXI-Lite: Q1.15 feedback amount
)
{
#pragma HLS INTERFACE axis      port=x_stream
#pragma HLS INTERFACE axis      port=y_stream
#pragma HLS INTERFACE s_axilite port=delay
#pragma HLS INTERFACE s_axilite port=feedback
#pragma HLS INTERFACE s_axilite port=return   // ap_ctrl_hs on the block level

    ap_axiu<16,0,0,0> in_samp, out_samp;

    // Process samples until the upstream DMA asserts TLAST
    do {
#pragma HLS PIPELINE II=1
        x_stream.read(in_samp);
        data_t x = (data_t)(int16_t)in_samp.data;

        int echo_idx = current_index - (int)delay;
        if (echo_idx < 0) echo_idx += 48000;

        data_t    delayed = delay_buffer[echo_idx];
        int32_t   mixed   = (int32_t)x +
                            ((int32_t)delayed * (int32_t)feedback >> 15);

        if (mixed >  32767) mixed =  32767;
        if (mixed < -32768) mixed = -32768;

        delay_buffer[current_index] = x;
        current_index = (current_index + 1) % 48000;

        out_samp.data = (ap_uint<16>)(int16_t)mixed;
        out_samp.last = in_samp.last;   // propagate TLAST
        out_samp.keep = in_samp.keep;
        y_stream.write(out_samp);

    } while (!in_samp.last);
}
