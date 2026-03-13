#ifndef AUDIOFX_ECHO_H
#define AUDIOFX_ECHO_H

#include <stdint.h>

// Audio sample: 16-bit signed
typedef int16_t data_t;

// Number of samples to delay
typedef int32_t delay_t;

// Feedback amount
typedef int16_t feedback_t;

void echo(
    data_t x,
    data_t *y,
    delay_t delay,
    feedback_t feedback);

#endif // AUDIOFX_ECHO_H
