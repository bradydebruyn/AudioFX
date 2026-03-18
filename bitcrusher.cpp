// bitcrusher.cpp
// Bitcrusher Effect
//
// Algorithm: y[n] = x[n] & ~((1 << bits_to_crush) - 1)
// - Zeros out the lowest `bits_to_crush` bits of each sample
// - Reduces effective bit depth, creating a lo-fi / quantization distortion
// - Pure bitwise AND — maps to trivial combinational logic (no DSP slices needed)
//
// Example: bits_to_crush = 4 on a 16-bit sample
//   input:  0b 1010 1111 0011 1100
//   mask:   0b 1111 1111 1111 0000
//   output: 0b 1010 1111 0011 0000  (LSBs zeroed)
//
// Fixed-point representation: 16-bit signed audio samples

#include "bitcrusher.h"

void bitcrusher(
    data_t x,             // input sample
    data_t *y,            // output sample
    crush_t bits_to_crush // number of LSBs to zero out (1–14 for 16-bit audio)
)
{
#pragma HLS INTERFACE ap_none port = x
#pragma HLS INTERFACE ap_none port = y
#pragma HLS INTERFACE ap_none port = bits_to_crush
#pragma HLS PIPELINE II = 1

    // Guard: if bits_to_crush is 0, pass through unchanged
    if (bits_to_crush == 0)
    {
        *y = x;
        return;
    }

    // Build mask: all ones except the bottom `bits_to_crush` bits
    // e.g., bits_to_crush=4 -> mask = 0xFFF0
    // Cast to uint16_t for the shift, then back to signed for audio arithmetic
    uint16_t mask = (uint16_t)(~((uint16_t)((1u << bits_to_crush) - 1u)));

    // Apply mask — this is a bitwise AND, pure combinational logic
    *y = (data_t)((uint16_t)x & mask);
}
