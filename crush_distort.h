// crush_distort.h
#ifndef CRUSH_DISTORT_H
#define CRUSH_DISTORT_H

#include <stdint.h>

// Audio sample: 16-bit signed (matches ADAU1761 codec output)
typedef int16_t data_t;

// Bitcrusher: number of bits to crush
typedef uint8_t crush_t;

// Distortion: pre-gain factor
typedef uint8_t gain_t;

// Distortion: wider accumulator for pre-gain result
typedef int32_t acc_t;

void crush_distort(
    data_t x,              // input sample
    data_t *y,             // output sample
    crush_t bits_to_crush, // bitcrusher parameter
    gain_t pre_gain,       // distortion parameter
    data_t threshold       // distortion parameter
);

#endif
