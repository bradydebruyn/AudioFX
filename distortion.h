// distortion.h
#ifndef DISTORTION_H
#define DISTORTION_H

#include <stdint.h>
#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

// Audio sample: 16-bit signed (matches ADAU1761 codec output)
typedef int16_t data_t;

// Pre-gain factor: small unsigned integer (1–16 is a reasonable range)
typedef uint8_t gain_t;

// Wider accumulator to hold pre-gain result without overflow
typedef int32_t acc_t;

void distortion(
    hls::stream<ap_axiu<16,0,0,0>> &x_stream,
    hls::stream<ap_axiu<16,0,0,0>> &y_stream,
    gain_t  pre_gain,
    data_t  threshold
);

#endif // DISTORTION_H
