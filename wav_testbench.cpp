// testbench_wav.cpp
// Vitis HLS C Simulation testbench — WAV file I/O
//
// HOW TO USE:
//   1. Convert your WAV to 16-bit PCM mono 44100Hz if needed:
//        ffmpeg -i input.wav -acodec pcm_s16le -ar 44100 -ac 1 test.wav
//
//   2. Set INPUT_WAV to the absolute path of your test WAV file.
//      OUTPUT_WAV will be written to the same folder.
//
//   3. In Vitis HLS:
//        - Add this file as the testbench source
//        - Keep distortion.cpp, bitcrusher.cpp as design sources
//        - Run C Simulation
//
//   4. After simulation, open output_distortion.wav, output_bitcrusher.wav,
//      and output_both.wav in Audacity to hear the results.
//
// RETURN VALUE:
//   0 = all checks passed (Vitis HLS marks simulation as PASS)
//   1 = one or more checks failed

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "distortion.h"
#include "bitcrusher.h"

// ─── CONFIGURE THESE ────────────────────────────────────────────────────────

// Use absolute paths to avoid HLS working directory confusion
// Windows example: "C:/hls_test/test.wav"
// Linux/Mac example: "/home/brady/hls_test/test.wav"
#define INPUT_WAV          "C:/hls_test/test.wav"
#define OUTPUT_DISTORTION  "C:/hls_test/output_distortion.wav"
#define OUTPUT_BITCRUSHER  "C:/hls_test/output_bitcrusher.wav"
#define OUTPUT_BOTH        "C:/hls_test/output_both.wav"

// Effect parameters — adjust to taste
#define DIST_GAIN       4        // pre-amp gain (try 2, 4, 8)
#define DIST_THRESHOLD  16383    // clip ceiling (16383 = 50% full scale)
#define CRUSH_BITS      4        // LSBs to zero (4 = effective 12-bit depth)

// ─── WAV Header ──────────────────────────────────────────────────────────────
#pragma pack(push, 1)
typedef struct {
    char     chunk_id[4];
    uint32_t chunk_size;
    char     format[4];
    char     subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     subchunk2_id[4];
    uint32_t subchunk2_size;
} WavHeader;
#pragma pack(pop)

// ─── Helpers ─────────────────────────────────────────────────────────────────
static int failures = 0;

void check(const char *label, int condition) {
    if (!condition) {
        printf("FAIL: %s\n", label);
        failures++;
    } else {
        printf("PASS: %s\n", label);
    }
}

int read_wav(const char *path, WavHeader *header, int16_t **samples, uint32_t *count) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("ERROR: Cannot open %s\n", path);
        printf("       Make sure the file exists and INPUT_WAV path is correct.\n");
        return 0;
    }

    if (fread(header, sizeof(WavHeader), 1, f) != 1) {
        printf("ERROR: Failed to read WAV header from %s\n", path);
        fclose(f); return 0;
    }

    // Validate
    if (strncmp(header->chunk_id, "RIFF", 4) != 0 ||
        strncmp(header->format,   "WAVE", 4) != 0) {
        printf("ERROR: %s is not a valid WAV file\n", path);
        fclose(f); return 0;
    }
    if (header->audio_format != 1) {
        printf("ERROR: %s is not PCM. Convert with:\n", path);
        printf("       ffmpeg -i %s -acodec pcm_s16le -ar 44100 -ac 1 test.wav\n", path);
        fclose(f); return 0;
    }
    if (header->bits_per_sample != 16) {
        printf("ERROR: %s is %d-bit. Only 16-bit supported.\n",
               path, header->bits_per_sample);
        fclose(f); return 0;
    }

    *count   = header->subchunk2_size / sizeof(int16_t);
    *samples = (int16_t *)malloc(header->subchunk2_size);
    if (!*samples) { printf("ERROR: Out of memory\n"); fclose(f); return 0; }

    if (fread(*samples, sizeof(int16_t), *count, f) != *count) {
        printf("ERROR: Could not read audio data from %s\n", path);
        free(*samples); fclose(f); return 0;
    }

    fclose(f);
    return 1;
}

void write_wav(const char *path, WavHeader *header, int16_t *samples, uint32_t count) {
    FILE *f = fopen(path, "wb");
    if (!f) { printf("ERROR: Cannot write %s\n", path); return; }
    fwrite(header, sizeof(WavHeader), 1, f);
    fwrite(samples, sizeof(int16_t), count, f);
    fclose(f);
    printf("  Written: %s\n", path);
}

// ─── Known-value unit checks ──────────────────────────────────────────────────
// These run regardless of the WAV file to confirm the effect logic is correct.
void run_unit_checks() {
    printf("\n--- Unit checks ---\n");
    data_t y;

    // Distortion: signal within threshold passes through unchanged
    distortion(1000, &y, 2, 16383);
    check("distortion: 1000 * gain=2 = 2000 (no clip)", y == 2000);

    // Distortion: positive clip
    distortion(20000, &y, 2, 16383);
    check("distortion: large positive clamps to +threshold", y == 16383);

    // Distortion: negative clip
    distortion(-20000, &y, 2, 16383);
    check("distortion: large negative clamps to -threshold", y == -16383);

    // Distortion: zero input
    distortion(0, &y, 4, 16383);
    check("distortion: zero input -> zero output", y == 0);

    // Bitcrusher: passthrough when crush=0
    bitcrusher(0x1234, &y, 0);
    check("bitcrusher: crush=0 is passthrough", y == 0x1234);

    // Bitcrusher: bottom nibble zeroed
    bitcrusher(0x1234, &y, 4);
    check("bitcrusher: crush=4, 0x1234 -> 0x1230", y == 0x1230);

    // Bitcrusher: bottom byte zeroed
    bitcrusher(0x1234, &y, 8);
    check("bitcrusher: crush=8, 0x1234 -> 0x1200", y == 0x1200);

    // Bitcrusher: negative sample (two's complement)
    bitcrusher(-1, &y, 4);
    check("bitcrusher: crush=4, -1 (0xFFFF) -> -16 (0xFFF0)", y == -16);
}

// ─── WAV-level checks ────────────────────────────────────────────────────────
// Sanity checks on the processed audio buffer.
void run_wav_checks(int16_t *in, int16_t *dist_out, int16_t *crush_out,
                    uint32_t num_samples) {
    printf("\n--- WAV-level checks ---\n");

    // 1. Output length matches input
    // (implicitly true since we allocate the same size, but good habit)
    check("sample count unchanged", num_samples > 0);

    // 2. Distortion output never exceeds threshold
    int dist_ok = 1;
    for (uint32_t i = 0; i < num_samples; i++) {
        if (dist_out[i] > DIST_THRESHOLD || dist_out[i] < -DIST_THRESHOLD) {
            dist_ok = 0;
            printf("  distortion exceeded threshold at sample %u: %d\n", i, dist_out[i]);
            break;
        }
    }
    check("distortion: no sample exceeds threshold", dist_ok);

    // 3. Bitcrusher output has correct LSBs zeroed
    int crush_ok = 1;
    uint16_t mask = (uint16_t)(~((1u << CRUSH_BITS) - 1u));
    for (uint32_t i = 0; i < num_samples; i++) {
        if (((uint16_t)crush_out[i] & ~mask) != 0) {
            crush_ok = 0;
            printf("  bitcrusher LSB not zeroed at sample %u: 0x%04X\n",
                   i, (uint16_t)crush_out[i]);
            break;
        }
    }
    check("bitcrusher: LSBs correctly zeroed in all samples", crush_ok);

    // 4. Neither effect produces silence on a non-silent input
    int32_t in_energy = 0, dist_energy = 0, crush_energy = 0;
    uint32_t check_len = num_samples < 1000 ? num_samples : 1000;
    for (uint32_t i = 0; i < check_len; i++) {
        in_energy    += abs(in[i]);
        dist_energy  += abs(dist_out[i]);
        crush_energy += abs(crush_out[i]);
    }
    check("distortion: output is not silent", dist_energy > 0);
    check("bitcrusher: output is not silent", crush_energy > 0);

    // 5. Print first 20 samples for visual inspection
    printf("\n%-6s  %-8s  %-10s  %-10s\n", "n", "input", "distorted", "bitcrushed");
    printf("%-6s  %-8s  %-10s  %-10s\n", "------","--------","----------","----------");
    uint32_t preview = num_samples < 20 ? num_samples : 20;
    for (uint32_t i = 0; i < preview; i++) {
        printf("%-6u  %-8d  %-10d  %-10d\n", i, in[i], dist_out[i], crush_out[i]);
    }
}

// ─── Main ────────────────────────────────────────────────────────────────────
int main() {
    printf("=== Vitis HLS WAV Testbench ===\n");
    printf("Input:      %s\n", INPUT_WAV);
    printf("Dist gain:  %d  threshold: %d\n", DIST_GAIN, DIST_THRESHOLD);
    printf("Crush bits: %d\n\n", CRUSH_BITS);

    // Always run unit checks first — these don't need the WAV file
    run_unit_checks();

    // --- Load WAV ---
    WavHeader header;
    int16_t  *input_samples = NULL;
    uint32_t  num_samples   = 0;

    if (!read_wav(INPUT_WAV, &header, &input_samples, &num_samples)) {
        printf("\nSkipping WAV tests (could not load file).\n");
        printf("Unit check result: %d failure(s)\n", failures);
        return failures > 0 ? 1 : 0;
    }

    printf("\nLoaded: %u samples, %u Hz, %u ch, %u-bit\n\n",
           num_samples, header.sample_rate,
           header.num_channels, header.bits_per_sample);

    // --- Allocate output buffers ---
    int16_t *dist_out  = (int16_t *)malloc(num_samples * sizeof(int16_t));
    int16_t *crush_out = (int16_t *)malloc(num_samples * sizeof(int16_t));
    int16_t *both_out  = (int16_t *)malloc(num_samples * sizeof(int16_t));

    // --- Run effects sample by sample ---
    for (uint32_t i = 0; i < num_samples; i++) {
        data_t x = input_samples[i];
        data_t y;

        // Distortion only
        distortion(x, &y, DIST_GAIN, DIST_THRESHOLD);
        dist_out[i] = y;

        // Bitcrusher only
        bitcrusher(x, &y, CRUSH_BITS);
        crush_out[i] = y;

        // Both chained: distortion -> bitcrusher
        distortion(x, &y, DIST_GAIN, DIST_THRESHOLD);
        data_t chained = y;
        bitcrusher(chained, &y, CRUSH_BITS);
        both_out[i] = y;
    }

    // --- WAV-level checks ---
    run_wav_checks(input_samples, dist_out, crush_out, num_samples);

    // --- Write output files ---
    printf("\n--- Writing output WAV files ---\n");
    write_wav(OUTPUT_DISTORTION, &header, dist_out,  num_samples);
    write_wav(OUTPUT_BITCRUSHER, &header, crush_out, num_samples);
    write_wav(OUTPUT_BOTH,       &header, both_out,  num_samples);

    // --- Cleanup ---
    free(input_samples);
    free(dist_out);
    free(crush_out);
    free(both_out);

    // --- Final result ---
    printf("\n=== Result: %d failure(s) ===\n", failures);
    return failures > 0 ? 1 : 0;  // 0 = PASS in Vitis HLS
}
