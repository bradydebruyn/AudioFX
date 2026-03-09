// distortion.h
#ifndef DISTORTION_H
#define DISTORTION_H

#include <stdint.h>

// Audio sample: 16-bit signed (matches ADAU1761 codec output)
typedef int16_t data_t;

// Pre-gain factor: small unsigned integer (1–16 is a reasonable range)
typedef uint8_t gain_t;

// Wider accumulator to hold pre-gain result without overflow
// 16-bit sample * 8-bit gain = needs up to 24 bits; use 32 to be safe
typedef int32_t acc_t;

void distortion(
    data_t  x,
    data_t *y,
    gain_t  pre_gain,
    data_t  threshold
);

#endif // DISTORTION_H
