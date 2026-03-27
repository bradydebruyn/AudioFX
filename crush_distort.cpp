// crush_distort.cpp
// Combined two effects: Bitcrusher -> Distortion

#include "crush_distort.h"

void crush_distort(
    data_t x,              // input sample
    data_t *y,             // output sample
    crush_t bits_to_crush, // bitcrusher
    gain_t pre_gain,       // distortion: pre-amp gain
    data_t threshold       // distortion: clipping ceiling
)
{
#pragma HLS INTERFACE s_axilite port = x
#pragma HLS INTERFACE s_axilite port = y
#pragma HLS INTERFACE s_axilite port = bits_to_crush
#pragma HLS INTERFACE s_axilite port = pre_gain
#pragma HLS INTERFACE s_axilite port = threshold
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS PIPELINE II = 1

    data_t bitcrusher_out;

    // bitcrusher
    if (bits_to_crush == 0)
    {
        bitcrusher_out = x;
    }
    else
    {
        uint16_t mask = (uint16_t)(~((uint16_t)((1u << bits_to_crush) - 1u)));
        bitcrusher_out = (data_t)((uint16_t)x & mask);
    }

    // distortion
    acc_t amplified = (acc_t)bitcrusher_out * (acc_t)pre_gain;

    if (amplified > (acc_t)threshold)
    {
        *y = threshold;
    }
    else if (amplified < -(acc_t)threshold)
    {
        *y = -threshold;
    }
    else
    {
        *y = (data_t)amplified;
    }
}
