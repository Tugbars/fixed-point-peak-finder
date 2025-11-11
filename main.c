/*!
 * Test Suite for embedded-signal-peaks
 *
 * Author: Tugbars Heptaskin
 * Date: 11/11/2025
 * Company: Aminic Aps
 *
 * Simple test cases to validate fixed-point peak detection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "embedded-signal-peaks.h"

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macro for test assertions */
#define TEST_ASSERT(condition, test_name) \
    do { \
        if (condition) { \
            printf("✓ PASS: %s\n", test_name); \
            tests_passed++; \
        } else { \
            printf("✗ FAIL: %s\n", test_name); \
            tests_failed++; \
        } \
    } while(0)

/*!
 * @brief Print signal for debugging
 */
static void print_signal(const char* name, const int16_t signal[], int32_t length)
{
    printf("%s: [", name);
    for (int32_t i = 0; i < length; i++) {
        printf("%d", signal[i]);
        if (i < length - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

/*!
 * @brief Test 1: Simple single peak
 */
static void test_simple_peak(void)
{
    printf("\n=== Test 1: Simple Single Peak ===\n");
    
    /* Triangle wave: peak at index 4 */
    int16_t signal[] = {10, 30, 50, 70, 90, 70, 50, 30, 10};
    int32_t length = 9;
    int32_t peak_idx = -1;
    
    print_signal("Signal", signal, length);
    
    PeakResultFP result = find_prominent_peak_fp(signal, length, &peak_idx, NULL);
    
    printf("Result: %s, Peak index: %d\n", 
           (result == PEAK_FP_OK) ? "OK" : "FAILED", peak_idx);
    
    if (result == PEAK_FP_OK) {
        float prominence = get_peak_prominence_float(signal, length, peak_idx);
        printf("Peak value: %d, Prominence: %.2f\n", 
               signal[peak_idx], prominence);
    }
    
    TEST_ASSERT(result == PEAK_FP_OK && peak_idx == 4, 
                "Single peak detection");
    TEST_ASSERT(signal[peak_idx] == 90, 
                "Peak value is correct");
}

/*!
 * @brief Test 2: Multiple peaks - select most prominent
 */
static void test_multiple_peaks(void)
{
    printf("\n=== Test 2: Multiple Peaks (Prominence) ===\n");
    
    /* Signal with two peaks: 80 at index 3, 100 at index 7
     * Peak at 100 should have higher prominence */
    int16_t signal[] = {10, 40, 70, 80, 60, 40, 70, 100, 50, 20};
    int32_t length = 10;
    int32_t peak_idx = -1;
    
    print_signal("Signal", signal, length);
    
    PeakResultFP result = find_prominent_peak_fp(signal, length, &peak_idx, NULL);
    
    printf("Result: %s, Peak index: %d\n", 
           (result == PEAK_FP_OK) ? "OK" : "FAILED", peak_idx);
    
    if (result == PEAK_FP_OK) {
        float prominence = get_peak_prominence_float(signal, length, peak_idx);
        printf("Peak value: %d, Prominence: %.2f\n", 
               signal[peak_idx], prominence);
    }
    
    TEST_ASSERT(result == PEAK_FP_OK && peak_idx == 7, 
                "Most prominent peak selected");
}

/*!
 * @brief Test 3: MATLAB-compatible prominence example
 * 
 * This tests the corrected topological prominence calculation.
 * Signal: [0, 10, 5, 20, 5, 15, 0]
 * Peak at index 3 (value 20) should have prominence ~20
 * (not 15 as naive implementation would calculate)
 */
static void test_matlab_prominence(void)
{
    printf("\n=== Test 3: MATLAB-Compatible Prominence ===\n");
    
    int16_t signal[] = {0, 10, 5, 20, 5, 15, 0};
    int32_t length = 7;
    int32_t peak_idx = -1;
    
    print_signal("Signal", signal, length);
    
    PeakResultFP result = find_prominent_peak_fp(signal, length, &peak_idx, NULL);
    
    printf("Result: %s, Peak index: %d\n", 
           (result == PEAK_FP_OK) ? "OK" : "FAILED", peak_idx);
    
    if (result == PEAK_FP_OK) {
        float prominence = get_peak_prominence_float(signal, length, peak_idx);
        printf("Peak value: %d, Prominence: %.2f\n", 
               signal[peak_idx], prominence);
        printf("Expected: Peak at index 3, prominence ~20.0\n");
        
        TEST_ASSERT(peak_idx == 3, "Peak at correct index");
        TEST_ASSERT(fabs(prominence - 20.0f) < 1.0f, 
                    "Prominence is MATLAB-compatible (~20, not ~15)");
    }
}

/*!
 * @brief Test 4: Noisy signal with clear peak
 */
static void test_noisy_signal(void)
{
    printf("\n=== Test 4: Noisy Signal ===\n");
    
    /* Peak at index 50 with additive noise */
    int16_t signal[100];
    int32_t length = 100;
    int32_t peak_idx = -1;
    
    /* Generate synthetic noisy signal */
    for (int32_t i = 0; i < length; i++) {
        /* Gaussian-like peak centered at 50 */
        float x = (float)(i - 50) / 10.0f;
        float base = 100.0f * expf(-x * x);
        /* Add noise */
        float noise = (float)((rand() % 21) - 10); /* ±10 */
        signal[i] = (int16_t)(base + noise);
    }
    
    printf("Noisy signal: 100 samples, peak around index 50\n");
    
    PeakResultFP result = find_prominent_peak_fp(signal, length, &peak_idx, NULL);
    
    printf("Result: %s, Peak index: %d\n", 
           (result == PEAK_FP_OK) ? "OK" : "FAILED", peak_idx);
    
    if (result == PEAK_FP_OK) {
        float prominence = get_peak_prominence_float(signal, length, peak_idx);
        printf("Peak value: %d, Prominence: %.2f\n", 
               signal[peak_idx], prominence);
    }
    
    /* Peak should be within ±5 samples of center */
    TEST_ASSERT(result == PEAK_FP_OK && 
                peak_idx >= 45 && peak_idx <= 55, 
                "Peak detected in noisy signal");
}

/*!
 * @brief Test 5: Edge cases
 */
static void test_edge_cases(void)
{
    printf("\n=== Test 5: Edge Cases ===\n");
    
    /* Case 1: Peak at start */
    printf("Case 5a: Peak at boundary (start)\n");
    int16_t signal1[] = {100, 80, 60, 40, 20};
    int32_t peak1 = -1;
    PeakResultFP result1 = find_prominent_peak_fp(signal1, 5, &peak1, NULL);
    TEST_ASSERT(result1 == PEAK_FP_OK && peak1 == 0, 
                "Peak at start detected");
    
    /* Case 2: Peak at end */
    printf("Case 5b: Peak at boundary (end)\n");
    int16_t signal2[] = {20, 40, 60, 80, 100};
    int32_t peak2 = -1;
    PeakResultFP result2 = find_prominent_peak_fp(signal2, 5, &peak2, NULL);
    TEST_ASSERT(result2 == PEAK_FP_OK && peak2 == 4, 
                "Peak at end detected");
    
    /* Case 3: Flat signal (no peak) */
    printf("Case 5c: Flat signal (no peak)\n");
    int16_t signal3[] = {50, 50, 50, 50, 50};
    int32_t peak3 = -1;
    PeakResultFP result3 = find_prominent_peak_fp(signal3, 5, &peak3, NULL);
    TEST_ASSERT(result3 == PEAK_FP_NO_PEAK_FOUND, 
                "No peak in flat signal");
    
    /* Case 4: Very short signal */
    printf("Case 5d: Very short signal\n");
    int16_t signal4[] = {10, 20};
    int32_t peak4 = -1;
    PeakResultFP result4 = find_prominent_peak_fp(signal4, 2, &peak4, NULL);
    TEST_ASSERT(result4 == PEAK_FP_BUFFER_TOO_SMALL, 
                "Short signal handled");
}

/*!
 * @brief Test 6: Custom configuration
 */
static void test_custom_config(void)
{
    printf("\n=== Test 6: Custom Configuration ===\n");
    
    /* Signal with small peak (prominence < 1.0) */
    int16_t signal[] = {10, 15, 20, 25, 20, 15, 10};
    int32_t length = 7;
    int32_t peak_idx = -1;
    
    print_signal("Signal", signal, length);
    
    /* Test with default config (should NOT find peak, prominence too low) */
    PeakResultFP result1 = find_prominent_peak_fp(signal, length, &peak_idx, NULL);
    printf("Default config result: %s\n", 
           (result1 == PEAK_FP_NO_PEAK_FOUND) ? "NO PEAK" : "FOUND");
    
    /* Test with relaxed config */
    PeakConfigFP custom_config = {
        .prominence_threshold_q16 = (int32_t)(0.1f * Q16_ONE), /* Lower threshold */
        .gradient_threshold_q16 = (int32_t)(0.05f * Q16_ONE),
        .noise_floor_q16 = (int32_t)(5.0f * Q16_ONE)
    };
    
    PeakResultFP result2 = find_prominent_peak_fp(signal, length, &peak_idx, &custom_config);
    printf("Custom config result: %s, Peak index: %d\n",
           (result2 == PEAK_FP_OK) ? "OK" : "FAILED", peak_idx);
    
    TEST_ASSERT(result1 == PEAK_FP_NO_PEAK_FOUND, 
                "Default config filters small peaks");
    TEST_ASSERT(result2 == PEAK_FP_OK && peak_idx == 3, 
                "Custom config detects small peaks");
}

/*!
 * @brief Test 7: Real-world ADC-like data
 */
static void test_adc_data(void)
{
    printf("\n=== Test 7: Simulated ADC Data ===\n");
    
    /* Simulate 12-bit ADC reading a sensor pulse */
    int16_t signal[128];
    int32_t length = 128;
    
    /* Generate pulse + baseline + noise */
    for (int32_t i = 0; i < length; i++) {
        int16_t baseline = 512; /* 12-bit ADC midpoint */
        int16_t noise = (int16_t)((rand() % 41) - 20); /* ±20 ADC counts */
        
        /* Pulse centered at sample 64 */
        if (i >= 50 && i <= 78) {
            float x = (float)(i - 64) / 8.0f;
            int16_t pulse = (int16_t)(800.0f * expf(-x * x));
            signal[i] = baseline + pulse + noise;
        } else {
            signal[i] = baseline + noise;
        }
    }
    
    int32_t peak_idx = -1;
    PeakResultFP result = find_prominent_peak_fp(signal, length, &peak_idx, NULL);
    
    printf("ADC data: 128 samples, 12-bit resolution\n");
    printf("Result: %s, Peak index: %d\n", 
           (result == PEAK_FP_OK) ? "OK" : "FAILED", peak_idx);
    
    if (result == PEAK_FP_OK) {
        printf("Peak value: %d ADC counts\n", signal[peak_idx]);
        float prominence = get_peak_prominence_float(signal, length, peak_idx);
        printf("Prominence: %.2f ADC counts\n", prominence);
    }
    
    TEST_ASSERT(result == PEAK_FP_OK && 
                peak_idx >= 60 && peak_idx <= 68, 
                "ADC pulse detected");
}

/*!
 * @brief Main test runner
 */
int main(void)
{
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  embedded-signal-peaks Test Suite         ║\n");
    printf("║  Fixed-Point Peak Detection Validation    ║\n");
    printf("╚════════════════════════════════════════════╝\n");
    
    /* Seed random number generator */
    srand(12345);
    
    /* Run test suite */
    test_simple_peak();
    test_multiple_peaks();
    test_matlab_prominence();
    test_noisy_signal();
    test_edge_cases();
    test_custom_config();
    test_adc_data();
    
    /* Print summary */
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  Test Results Summary                      ║\n");
    printf("╠════════════════════════════════════════════╣\n");
    printf("║  Passed: %-3d                               ║\n", tests_passed);
    printf("║  Failed: %-3d                               ║\n", tests_failed);
    printf("║  Total:  %-3d                               ║\n", tests_passed + tests_failed);
    printf("╚════════════════════════════════════════════╝\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
