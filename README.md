# embedded-signal-peaks

MISRA-compliant fixed-point peak detection for embedded systems with topological prominence calculation.

## Features

- **Fixed-Point Arithmetic**: Q16.16 format for efficient embedded processing
- **MATLAB-Compatible Prominence**: True topological prominence calculation
- **Gradient-Based Detection**: Central difference method with zero-crossing detection
- **Low Memory Footprint**: ~2KB static buffers, minimal stack usage
- **MISRA C Compliant**: No recursion, explicit types, defensive checks
- **Thread-Safe Option**: Caller-provided buffers available
- **Configurable**: Adjustable thresholds for noise floor, prominence, and gradients

## Use Cases

- Sensor signal processing (accelerometers, magnetometers, pressure sensors)
- ADC data analysis (pulse detection, waveform analysis)
- Motor control feedback (encoder peaks, position sensing)
- Biomedical signals (heart rate, respiration detection)

## Quick Start

### Basic Usage
```c
#include "embedded-signal-peaks.h"

int16_t adc_samples[256];  /* Your 16-bit ADC data */
int32_t peak_index;

/* Find the most prominent peak */
PeakResultFP result = find_prominent_peak_fp(adc_samples, 256, &peak_index, NULL);

if (result == PEAK_FP_OK) {
    int16_t peak_value = adc_samples[peak_index];
    printf("Peak at index %d, value %d\n", peak_index, peak_value);
}
```

### Custom Configuration
```c
PeakConfigFP config = {
    .prominence_threshold_q16 = (int32_t)(0.5f * Q16_ONE),  /* Lower threshold */
    .gradient_threshold_q16 = (int32_t)(0.05f * Q16_ONE),   /* More sensitive */
    .noise_floor_q16 = (int32_t)(5.0f * Q16_ONE)            /* Noise filtering */
};

PeakResultFP result = find_prominent_peak_fp(signal, length, &peak_index, &config);
```

### Thread-Safe Usage
```c
/* Allocate your own buffers for thread-safety */
int32_t signal_buffer[MAX_SIGNAL_LENGTH];
int32_t peaks_buffer[MAX_PEAKS];

PeakResultFP result = find_prominent_peak_fp_buffered(
    signal, length, &peak_index, NULL,
    signal_buffer, peaks_buffer
);
```

## API Reference

### Main Functions

#### `find_prominent_peak_fp()`
```c
PeakResultFP find_prominent_peak_fp(
    const int16_t signal[],
    int32_t length,
    int32_t *peak_index,
    const PeakConfigFP *user_config
);
```
Find the most prominent peak in a signal.

**Parameters:**
- `signal`: Input signal array (16-bit ADC samples)
- `length`: Signal length (must be ≤ MAX_SIGNAL_LENGTH)
- `peak_index`: Output pointer for peak index
- `user_config`: Optional configuration (NULL for defaults)

**Returns:**
- `PEAK_FP_OK`: Peak found successfully
- `PEAK_FP_NO_PEAK_FOUND`: No valid peak detected
- `PEAK_FP_INVALID_INPUT`: Invalid parameters
- `PEAK_FP_BUFFER_TOO_SMALL`: Signal too short (< 3 samples)

---

#### `find_prominent_peak_fp_buffered()`
```c
PeakResultFP find_prominent_peak_fp_buffered(
    const int16_t signal[],
    int32_t length,
    int32_t *peak_index,
    const PeakConfigFP *user_config,
    int32_t *signal_q16_buffer,
    int32_t *peaks_buffer
);
```
Thread-safe version with caller-provided buffers.

**Additional Parameters:**
- `signal_q16_buffer`: Buffer of `length` int32_t elements
- `peaks_buffer`: Buffer of `MAX_PEAKS` int32_t elements

---

#### `get_peak_prominence_float()`
```c
float get_peak_prominence_float(
    const int16_t signal[],
    int32_t length,
    int32_t peak_index
);
```
Get prominence value for debugging/validation (returns float).

### Configuration Structure
```c
typedef struct {
    int32_t prominence_threshold_q16;  /* Minimum prominence (Q16.16) */
    int32_t gradient_threshold_q16;    /* Minimum gradient for valid peak */
    int32_t noise_floor_q16;           /* Minimum peak height */
} PeakConfigFP;
```

**Default Values:**
- `prominence_threshold_q16`: 1.0 (65536 in Q16.16)
- `gradient_threshold_q16`: 0.1 (6554 in Q16.16)
- `noise_floor_q16`: 10.0 (655360 in Q16.16)

### Fixed-Point Helpers
```c
#define Q16_ONE (1L << 16)  /* 1.0 in Q16.16 = 65536 */

/* Convert float to Q16.16 */
int32_t value_q16 = (int32_t)(1.5f * Q16_ONE);  /* 1.5 → 98304 */
```

## Configuration

Adjust constants in `embedded-signal-peaks.c` for your system:
```c
#define MAX_SIGNAL_LENGTH (512)  /* Maximum signal samples */
#define MAX_PEAKS (32)           /* Maximum peaks to detect */
```

**Memory Usage:**
- Static buffers: ~2KB (2176 bytes)
- Stack per call: ~40 bytes

## Algorithm Overview

1. **Convert to Q16.16**: Input 16-bit samples → fixed-point
2. **Gradient Analysis**: Compute central difference derivatives
3. **Zero-Crossing Detection**: Find gradient sign changes (peaks)
4. **Topological Prominence**: MATLAB-compatible prominence calculation
   - Walk left/right until finding higher peaks
   - Reference level = max(left_min, right_min)
5. **Selection**: Return peak with highest prominence above threshold

### Why Topological Prominence?

Traditional peak detection often fails with multiple peaks. Topological prominence measures how "significant" a peak is by finding the minimum descent required to reach a higher peak.

**Example:**
```
Signal: [0, 10, 5, 20, 5, 15, 0]
               ^
              Peak at index 3 (value=20)
```

- **Naive**: Prominence = 20 - 5 = 15 (wrong!)
- **Topological**: Prominence = 20 - 0 = 20 (correct)

The algorithm walks until finding the boundary or a higher peak, ensuring accurate prominence even in complex signals.

## Requirements

- **Compiler**: C99 or later
- **Architecture**: Any (tested on ARM Cortex-M, x86)
- **Dependencies**: None (uses only standard `<stdint.h>`, `<stdbool.h>`)
- **MISRA**: Designed for MISRA C compliance

## Performance

**Typical Performance (ARM Cortex-M4 @ 168 MHz):**
- 256 samples: ~15 µs
- 512 samples: ~28 µs

**Complexity:**
- Gradient computation: O(n)
- Peak detection: O(n)
- Prominence calculation: O(n·p) where p = number of peaks (typically p << n)

## Examples

### Example 1: Sensor Pulse Detection
```c
/* Detect pressure sensor pulse */
int16_t pressure_data[200];
int32_t pulse_peak;

/* Configure for low-noise sensor */
PeakConfigFP config = {
    .prominence_threshold_q16 = (int32_t)(2.0f * Q16_ONE),
    .gradient_threshold_q16 = (int32_t)(0.2f * Q16_ONE),
    .noise_floor_q16 = (int32_t)(50.0f * Q16_ONE)
};

if (find_prominent_peak_fp(pressure_data, 200, &pulse_peak, &config) == PEAK_FP_OK) {
    process_pulse_timing(pulse_peak);
}
```

### Example 2: Heart Rate Monitor
```c
/* Detect R-peaks in ECG signal */
int16_t ecg_buffer[512];
int32_t r_peak_index;

/* R-peaks typically have high prominence */
PeakConfigFP ecg_config = {
    .prominence_threshold_q16 = (int32_t)(5.0f * Q16_ONE),
    .gradient_threshold_q16 = (int32_t)(1.0f * Q16_ONE),
    .noise_floor_q16 = (int32_t)(100.0f * Q16_ONE)
};

if (find_prominent_peak_fp(ecg_buffer, 512, &r_peak_index, &ecg_config) == PEAK_FP_OK) {
    calculate_heart_rate(r_peak_index);
}
```

## License

MIT

## Author

**Tugbars Heptaskin**  

## Contributing

Contributions welcome! Please ensure:
- MISRA C compliance maintained
- All tests pass (`make run`)
- Code documented with Doxygen-style comments

## Changelog

### Version 1.0.0 (2025-11-11)
- Initial release
- Q16.16 fixed-point implementation
- MATLAB-compatible topological prominence
- Comprehensive test suite
