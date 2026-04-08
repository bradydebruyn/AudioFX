// bitcrusher.h
#ifndef BITCRUSHER_H
#define BITCRUSHER_H

#include <stdint.h>
#include "hls_stream.h"
#include "ap_axi_sdata.h"

// Audio sample: 16-bit signed (matches ADAU1761 codec output)
typedef int32_t data_t;

// Number of bits to crush: small value (0–14 is meaningful for 16-bit audio)
typedef uint8_t crush_t;

void bitcrusher(
    data_t   x,
    data_t  *y,
    crush_t  bits_to_crush
);

#endif // BITCRUSHER_H
