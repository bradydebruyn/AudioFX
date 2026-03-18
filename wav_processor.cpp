// wav_processor.cpp
//
// Reads a 16-bit PCM WAV file, applies the selected effect(s),
// and writes the result to a new WAV file.
//
// Limitations:
//   - Only supports 16-bit signed PCM WAV (no compressed formats)
//   - Mono or stereo (stereo processes both channels identically)
//   - No MP3 support — use a tool like Audacity to convert MP3 -> WAV first
//
// Usage:
//   Swap the INPUT_FILE / OUTPUT_FILE defines, adjust effect parameters,
//   rebuild and run on the board or as a desktop simulation (Vitis HLS).

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "distortion.h"
#include "bitcrusher.h"
#include "echo.h"

// ─── WAV Header (From StackOverflow)  ─────────────────────────────────────────────────────────────
// Standard 44-byte PCM WAV header (little-endian)
#pragma pack(push, 1)
typedef struct
{
    char chunk_id[4];         // "RIFF"
    uint32_t chunk_size;      // file size - 8
    char format[4];           // "WAVE"
    char subchunk1_id[4];     // "fmt "
    uint32_t subchunk1_size;  // 16 for PCM
    uint16_t audio_format;    // 1 = PCM (uncompressed)
    uint16_t num_channels;    // 1 = mono, 2 = stereo
    uint32_t sample_rate;     // e.g. 44100, 48000
    uint32_t byte_rate;       // sample_rate * num_channels * bits/8
    uint16_t block_align;     // num_channels * bits/8
    uint16_t bits_per_sample; // 16
    char subchunk2_id[4];     // "data"
    uint32_t subchunk2_size;  // number of bytes of audio data
} WavHeader;
#pragma pack(pop)

// ─── Effect Selection ───────────────────────────────────────────────────────
typedef enum
{
    EFFECT_DISTORTION, // distortion only
    EFFECT_BITCRUSHER, // bitcrusher only
    EFFECT_BOTH,       // distortion first, then bitcrusher
    EFFECT_ECHO        // echo only
} EffectMode;

// ─── WAV Validation (From StackOverflow)  ─────────────────────────────────────────────────────────
int validate_wav(WavHeader *h)
{
    if (strncmp(h->chunk_id, "RIFF", 4) != 0)
    {
        printf("ERROR: Not a RIFF file\n");
        return 0;
    }
    if (strncmp(h->format, "WAVE", 4) != 0)
    {
        printf("ERROR: Not a WAVE file\n");
        return 0;
    }
    if (h->audio_format != 1)
    {
        printf("ERROR: Not PCM (compressed?)\n");
        return 0;
    }
    if (h->bits_per_sample != 16)
    {
        printf("ERROR: Only 16-bit PCM supported\n");
        return 0;
    }
    return 1;
}

// ─── Main Processor ─────────────────────────────────────────────────────────
int process_wav(
    const char *input_path,
    const char *output_path,
    EffectMode mode,

    // Distortion params
    gain_t dist_gain,
    data_t dist_threshold,

    // Bitcrusher params
    crush_t crush_bits,

    // Echo params
    delay_t echo_delay,
    feedback_t echo_feedback)
{
    // --- Open input ---
    FILE *fin = fopen(input_path, "rb");
    if (!fin)
    {
        printf("ERROR: Could not open input file: %s\n", input_path);
        return -1;
    }

    // --- Read and validate header ---
    WavHeader header;
    if (fread(&header, sizeof(WavHeader), 1, fin) != 1)
    {
        printf("ERROR: Could not read WAV header\n");
        fclose(fin);
        return -1;
    }
    if (!validate_wav(&header))
    {
        fclose(fin);
        return -1;
    }

    printf("Input:       %s\n", input_path);
    printf("Sample rate: %u Hz\n", header.sample_rate);
    printf("Channels:    %u\n", header.num_channels);
    printf("Bit depth:   %u-bit\n", header.bits_per_sample);

    uint32_t num_samples = header.subchunk2_size / sizeof(int16_t);
    printf("Samples:     %u (%.2f sec)\n\n",
           num_samples,
           (float)num_samples / (header.sample_rate * header.num_channels));

    // --- Read all audio data into buffer ---
    int16_t *audio = (int16_t *)malloc(header.subchunk2_size);
    if (!audio)
    {
        printf("ERROR: Out of memory\n");
        fclose(fin);
        return -1;
    }

    if (fread(audio, sizeof(int16_t), num_samples, fin) != num_samples)
    {
        printf("ERROR: Could not read audio data\n");
        free(audio);
        fclose(fin);
        return -1;
    }
    fclose(fin);

    // --- Process each sample through the effect(s) ---

    for (uint32_t i = 0; i < num_samples; i++)
    {
        data_t sample = audio[i];
        data_t out = sample;

        switch (mode)
        {
        case EFFECT_DISTORTION:
            distortion(sample, &out, dist_gain, dist_threshold);
            break;

        case EFFECT_BITCRUSHER:
            bitcrusher(sample, &out, crush_bits);
            break;

        case EFFECT_BOTH:
            distortion(sample, &out, dist_gain, dist_threshold);
            sample = out;
            bitcrusher(sample, &out, crush_bits);
            break;

        case EFFECT_ECHO:
            echo(sample, &out, echo_delay, echo_feedback);
            break;
        }

        audio[i] = out;
    }

    // --- Write output WAV (same header, processed data) ---
    FILE *fout = fopen(output_path, "wb");
    if (!fout)
    {
        printf("ERROR: Could not open output file: %s\n", output_path);
        free(audio);
        return -1;
    }

    fwrite(&header, sizeof(WavHeader), 1, fout);
    fwrite(audio, sizeof(int16_t), num_samples, fout);
    fclose(fout);
    free(audio);

    printf("Output written to: %s\n", output_path);
    return 0;
}

// ─── Entry Point ────────────────────────────────────────────────────────────
int main()
{
    // --- configure ---
    const char *INPUT_FILE = "input.wav";
    const char *OUTPUT_FILE = "output.wav";

    EffectMode mode = EFFECT_ECHO; // EFFECT_DISTORTION, EFFECT_BITCRUSHER, EFFECT_BOTH, or EFFECT_ECHO

    // Distortion parameters
    gain_t dist_gain = 4;          // pre-amp gain (try 2, 4, 8)
    data_t dist_threshold = 16383; // clip ceiling (16383 = 50% of full scale)

    // Bitcrusher parameters
    crush_t crush_bits = 14; // LSBs to zero (4 = drop to 12-bit effective depth)

    // Echo parameters
    delay_t echo_delay = 48000 / 2;   // 0.5 second delay
    feedback_t echo_feedback = 16384; // 50% feedback

    return process_wav(INPUT_FILE, OUTPUT_FILE, mode,
                       dist_gain, dist_threshold, crush_bits,
                       echo_delay, echo_feedback);
}
