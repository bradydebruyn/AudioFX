// combined_effects.h
#ifndef COMBINED_EFFECTS_H
#define COMBINED_EFFECTS_H

#include <stdint.h>

// Audio sample: 16-bit signed (matches ADAU1761 codec output)
typedef int16_t data_t;

// Bitcrusher: number of bits to crush
typedef uint8_t crush_t;

// Distortion: pre-gain factor
typedef uint8_t gain_t;

// Distortion: wider accumulator for pre-gain result
typedef int32_t acc_t;

// Echo: number of samples to delay
typedef int32_t delay_t;

// Echo: feedback amount
typedef int16_t feedback_t;

void combined_effects(
    data_t x,              // input sample
    data_t *y,             // output sample
    crush_t bits_to_crush, // bitcrusher parameter
    gain_t pre_gain,       // distortion parameter
    data_t threshold,      // distortion parameter
    delay_t delay,         // echo parameter
    feedback_t feedback    // echo parameter
);

#endif
