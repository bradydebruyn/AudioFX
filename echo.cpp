// echo.cpp
// Echo Effect
// For use in Vitis HLS

// methodology in this file is referenced from Microsoft Learn: https://learn.microsoft.com/en-us/previous-versions/windows/desktop/wmp/creating-the-echo-effect
// Microsoft uses algorithm: i = (int)((i * m_fDryMix ) + (delay * m_fWetMix));   where Drymix is percentage of original signal and WetMix is percentage of delayed signal.
// We impleemeneted a simpler algorithm, where the output is the sum of the input and a scaled version of the delayed signal.
// The same circular buffer methodology was used.

#include "echo.h"

static data_t delay_buffer[48000]; // 1 second buffer at 48kHz sample rate, make larger for longer delays
static int current_index = 0;

void echo(
    data_t x,           // input sample
    data_t *y,          // output sample
    delay_t delay,      // num samples to delay
    feedback_t feedback // feedback amount
)
{

#pragma HLS INTERFACE s_axilite port = x
#pragma HLS INTERFACE s_axilite port = y
#pragma HLS INTERFACE s_axilite port = delay
#pragma HLS INTERFACE s_axilite port = feedback
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS PIPELINE II = 1

    // using the current index and delay, calculate the index of the delayed sample
    int echo_value = current_index - delay;
    if (echo_value < 0)
    {
        echo_value += 48000; // wrap around the buffer if below zero
    }

    // retrieve the delayed sample from the buffer
    data_t delayed_sample = delay_buffer[echo_value];

    int32_t mixed_sample = x + ((int32_t)delayed_sample * (int32_t)feedback >> 15);

    if (mixed_sample > 32767)
    {
        mixed_sample = 32767;
    }
    if (mixed_sample < -32768)
    {
        mixed_sample = -32768;
    }

    *y = (data_t)mixed_sample;

    delay_buffer[current_index] = x;             // store the current input sample in the delay buffer
    current_index = (current_index + 1) % 48000; // move up the current index
}