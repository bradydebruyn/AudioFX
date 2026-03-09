// testbench.cpp
// Software testbench for distortion and bitcrusher effects
// Run this on the PS to verify correctness before moving to PL
//
// Tests:
//   1. Distortion: sine wave, constant values, edge cases (max, min, zero)
//   2. Bitcrusher: known bit patterns to confirm correct LSB masking
//
// Expected results are calculated analytically so you can spot check.

#include <stdio.h>
#include <stdint.h>
#include <math.h>       // for sinf(), M_PI
#include "distortion.h"
#include "bitcrusher.h"

// ─── Helpers ────────────────────────────────────────────────────────────────

// Simple pass/fail checker
static int failures = 0;
void check(const char *label, int16_t got, int16_t expected) {
    if (got != expected) {
        printf("FAIL [%s]: got %d, expected %d\n", label, got, expected);
        failures++;
    } else {
        printf("PASS [%s]: %d\n", label, got);
    }
}

// ─── Distortion Tests ───────────────────────────────────────────────────────

void test_distortion() {
    printf("\n=== Distortion Tests ===\n");
    data_t y;

    // --- Constant value tests ---
    // Input within threshold (no clipping)
    distortion(1000, &y, 2, 16383);
    check("dist: 1000 * gain=2, no clip", y, 2000);

    // Input exactly at threshold
    distortion(8192, &y, 2, 16383);
    check("dist: 8192 * gain=2 = 16384, clips to 16383", y, 16383);

    // Positive clip
    distortion(20000, &y, 2, 16383);
    check("dist: large positive clips to +threshold", y, 16383);

    // Negative clip
    distortion(-20000, &y, 2, 16383);
    check("dist: large negative clips to -threshold", y, -16383);

    // Zero input
    distortion(0, &y, 4, 16383);
    check("dist: zero input", y, 0);

    // Gain of 1 (unity), below threshold
    distortion(500, &y, 1, 16383);
    check("dist: gain=1 passthrough", y, 500);

    // --- Sine wave sweep ---
    printf("\n--- Distortion sine wave (gain=4, threshold=16383) ---\n");
    printf("%-6s  %-8s  %-8s\n", "n", "input", "output");
    for (int n = 0; n < 16; n++) {
        // Generate a 0–1 sine, scale to 16-bit range
        float sample_f = sinf(2.0f * (float)M_PI * n / 16.0f) * 10000.0f;
        data_t x = (data_t)sample_f;
        distortion(x, &y, 4, 16383);
        printf("%-6d  %-8d  %-8d\n", n, x, y);
    }
}

// ─── Bitcrusher Tests ───────────────────────────────────────────────────────

void test_bitcrusher() {
    printf("\n=== Bitcrusher Tests ===\n");
    data_t y;

    // bits_to_crush = 0 (passthrough)
    bitcrusher(0x1234, &y, 0);
    check("crush=0: passthrough", y, 0x1234);

    // bits_to_crush = 4 — bottom nibble zeroed
    // 0x1234 = 0001 0010 0011 0100 -> 0001 0010 0011 0000 = 0x1230
    bitcrusher(0x1234, &y, 4);
    check("crush=4: 0x1234 -> 0x1230", y, 0x1230);

    // bits_to_crush = 8 — bottom byte zeroed
    // 0x1234 -> 0x1200
    bitcrusher(0x1234, &y, 8);
    check("crush=8: 0x1234 -> 0x1200", y, 0x1200);

    // bits_to_crush = 1 — only LSB zeroed (rounds to even)
    // 0x0007 = 0000 0000 0000 0111 -> 0000 0000 0000 0110 = 0x0006
    bitcrusher(0x0007, &y, 1);
    check("crush=1: 7 -> 6 (LSB cleared)", y, 6);

    // Negative number (two's complement preserved)
    // -256 = 0xFF00, crush=4 -> mask=0xFFF0
    // 0xFF00 & 0xFFF0 = 0xFF00 (already aligned, no change)
    bitcrusher(-256, &y, 4);
    check("crush=4: -256 -> -256 (already aligned)", y, -256);

    // -1 = 0xFFFF, crush=4 -> 0xFFF0 = -16
    bitcrusher(-1, &y, 4);
    check("crush=4: -1 (0xFFFF) -> -16 (0xFFF0)", y, -16);

    // --- Stepped audio sweep to hear quantization effect ---
    printf("\n--- Bitcrusher sweep: bits_to_crush 0..8 on sample 0x7ABC ---\n");
    printf("%-6s  %-8s\n", "crush", "output");
    for (int b = 0; b <= 8; b++) {
        bitcrusher(0x7ABC, &y, (crush_t)b);
        printf("%-6d  0x%04X (%d)\n", b, (uint16_t)y, y);
    }
}

// ─── Main ───────────────────────────────────────────────────────────────────

int main() {
    test_distortion();
    test_bitcrusher();

    printf("\n=== Results: %d failure(s) ===\n", failures);
    return failures;   // Vitis HLS testbench convention: 0 = pass
}
