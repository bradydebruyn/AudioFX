
// bitcrusher.cpp  –  AXI-Stream version
// bits_to_crush stays as an AXI-Lite side-channel register.
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
    hls::stream<ap_axiu<32,0,0,0>> &x_stream,
    hls::stream<ap_axiu<32,0,0,0>> &y_stream,
    crush_t bits_to_crush
)
{
#pragma HLS INTERFACE axis      port=x_stream
#pragma HLS INTERFACE axis      port=y_stream
#pragma HLS INTERFACE s_axilite port=bits_to_crush
#pragma HLS INTERFACE s_axilite port=return

    ap_axiu<32,0,0,0> in_samp, out_samp;
    uint16_t mask = (bits_to_crush == 0)
                    ? 0xFFFF
                    : (uint32_t)(~((uint32_t)((1u << bits_to_crush) - 1u)));

    do {
#pragma HLS PIPELINE II=1
        x_stream.read(in_samp);
        data_t x = (data_t)(int32_t)in_samp.data;

        out_samp.data = (ap_uint<32>)((uint32_t)x & mask);
        out_samp.last = in_samp.last;
        out_samp.keep = in_samp.keep;
        y_stream.write(out_samp);

    } while (!in_samp.last);
}
