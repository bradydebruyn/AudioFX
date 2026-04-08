// distortion.cpp  –  AXI-Stream version
// pre_gain and threshold stay as AXI-Lite side-channel registers.
// Hard Clipping Distortion Effect
// For use in Vitis HLS
//
// Algorithm: y[n] = clip(g * x[n], -threshold, +threshold)
// - Pre-gain amplifies the signal before clipping
// - Threshold determines the clipping ceiling
// - Values exceeding the threshold are hard-clamped (comparator logic)
//
// Fixed-point representation: audio samples are 16-bit signed integers


#include "distortion.h"

void distortion(
    hls::stream<ap_axiu<32,0,0,0>> &x_stream,
    hls::stream<ap_axiu<32,0,0,0>> &y_stream,
    gain_t pre_gain,
    data_t threshold
)
{
#pragma HLS INTERFACE axis      port=x_stream
#pragma HLS INTERFACE axis      port=y_stream
#pragma HLS INTERFACE s_axilite port=pre_gain
#pragma HLS INTERFACE s_axilite port=threshold
#pragma HLS INTERFACE s_axilite port=return

    ap_axiu<32,0,0,0> in_samp, out_samp;

    do {
#pragma HLS PIPELINE II=1
        x_stream.read(in_samp);
        data_t x = (data_t)(int16_t)in_samp.data;

        acc_t amplified = (acc_t)x * (acc_t)pre_gain;
        data_t clipped;

        if      (amplified >  (acc_t)threshold) clipped =  threshold;
        else if (amplified < -(acc_t)threshold) clipped = -threshold;
        else                                    clipped =  (data_t)amplified;

        out_samp.data = (ap_uint<32>)(int32_t)clipped;
        out_samp.last = in_samp.last;
        out_samp.keep = in_samp.keep;
        y_stream.write(out_samp);

    } while (!in_samp.last);
}
