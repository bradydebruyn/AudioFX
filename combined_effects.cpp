
// combine effects: Bitcrusher -> Distortion -> Echo

#include "combined_effects.h"

static data_t delay_buffer[48000]; // 1 second buffer at 48kHz sample rate
static int current_index = 0;

void combined_effects(
    data_t x,              // input sample
    data_t *y,             // output sample
    crush_t bits_to_crush, // bitcrusher
    gain_t pre_gain,       // distortion: pre-amp gain
    data_t threshold,      // distortion
    delay_t delay,         // echo: samples to delay
    feedback_t feedback    // echo: feedback amount
)
{
#pragma HLS INTERFACE s_axilite port = x
#pragma HLS INTERFACE s_axilite port = y
#pragma HLS INTERFACE s_axilite port = bits_to_crush
#pragma HLS INTERFACE s_axilite port = pre_gain
#pragma HLS INTERFACE s_axilite port = threshold
#pragma HLS INTERFACE s_axilite port = delay
#pragma HLS INTERFACE s_axilite port = feedback
#pragma HLS INTERFACE ap_ctrl_none port = return
#pragma HLS PIPELINE II = 1

    data_t bitcrusher_out;
    data_t distortion_out;
    data_t echo_out;

    // bitcrusher
    if (bits_to_crush == 0)
    {
        bitcrusher_out = x;
    }
    else
    {
        uint16_t mask = (uint16_t)(~((uint16_t)((1u << bits_to_crush) - 1u)));
        bitcrusher_out = (data_t)((uint16_t)x & mask);
    }

    // distortion
    acc_t amplified = (acc_t)bitcrusher_out * (acc_t)pre_gain;

    if (amplified > (acc_t)threshold)
    {
        distortion_out = threshold;
    }
    else if (amplified < -(acc_t)threshold)
    {
        distortion_out = -threshold;
    }
    else
    {
        distortion_out = (data_t)amplified;
    }

    // echo
    int echo_index = current_index - delay;
    if (echo_index < 0)
    {
        echo_index += 48000;
    }

    data_t delayed_sample = delay_buffer[echo_index];

    int32_t mixed_sample = distortion_out + ((int32_t)delayed_sample * (int32_t)feedback >> 15);

    if (mixed_sample > 32767)
    {
        mixed_sample = 32767;
    }
    if (mixed_sample < -32768)
    {
        mixed_sample = -32768;
    }

    echo_out = (data_t)mixed_sample;

    delay_buffer[current_index] = distortion_out;
    current_index = (current_index + 1) % 48000;

    // final output
    *y = echo_out;
}
