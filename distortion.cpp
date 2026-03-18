// distortion.cpp
// Hard Clipping Distortion Effect
// For use in Vitis HLS
//
// Algorithm: y[n] = clip(g * x[n], -threshold, +threshold)
// - Pre-gain amplifies the signal before clipping
// - Threshold determines the clipping ceiling
// - Values exceeding the threshold are hard-clamped (comparator logic)
//
// Fixed-point representation: audio samples are 16-bit signed integers
// (as delivered by ADAU1761 codec, range -32768 to 32767)

#include "distortion.h"

void distortion(
    data_t x,        // input sample
    data_t *y,       // output sample
    gain_t pre_gain, // integer pre-amp gain factor (e.g., 2, 4, 8)
    data_t threshold // clipping ceiling (positive value, e.g., 16383 = 50%)
)
{
#pragma HLS INTERFACE ap_none port = x
#pragma HLS INTERFACE ap_none port = y
#pragma HLS INTERFACE ap_none port = pre_gain
#pragma HLS INTERFACE ap_none port = threshold
#pragma HLS PIPELINE II = 1

    // Step 1: Apply pre-gain using fixed-point multiplication
    // Use wider intermediate type to avoid overflow before clipping
    acc_t amplified = (acc_t)x * (acc_t)pre_gain;

    // Step 2: Hard clip — comparator logic maps directly to LUTs/DSP slices
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
