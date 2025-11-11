/*!
 * Fixed-Point Adaptive Peak Finding (Simplified, Corrected)
 *
 * Author: Tugbars Heptaskin  
 * Date: 11/11/2025
 *
 * MISRA C compliant with true MATLAB-compatible topological prominence.
 * Optimized for embedded systems with limited stack space.
 */

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

/* Q16.16 Fixed-Point Configuration */
#define Q16_SHIFT (16)
#define Q16_ONE (1L << Q16_SHIFT)
#define Q16_HALF (1L << (Q16_SHIFT - 1))

/* Buffer sizes - adjust for your system */
#define MAX_SIGNAL_LENGTH (512)
#define MAX_PEAKS (32)

/* Configuration constants (Q16.16 format) */
#define PROMINENCE_THRESHOLD_Q16 ((int32_t)(1.0f * Q16_ONE))
#define GRADIENT_THRESHOLD_Q16 ((int32_t)(0.1f * Q16_ONE))
#define NOISE_FLOOR_Q16 ((int32_t)(10.0f * Q16_ONE))

/* Return codes */
typedef enum {
    PEAK_FP_OK = 0,
    PEAK_FP_NO_PEAK_FOUND = 1,
    PEAK_FP_INVALID_INPUT = 2,
    PEAK_FP_BUFFER_TOO_SMALL = 3
} PeakResultFP;

/* Configuration struct */
typedef struct {
    int32_t prominence_threshold_q16;
    int32_t gradient_threshold_q16;
    int32_t noise_floor_q16;
} PeakConfigFP;

/* Default configuration */
static const PeakConfigFP default_config_fp = {
    PROMINENCE_THRESHOLD_Q16,
    GRADIENT_THRESHOLD_Q16,
    NOISE_FLOOR_Q16
};

/* Static buffers to reduce stack usage */
static int32_t s_signal_q16[MAX_SIGNAL_LENGTH];
static int32_t s_peak_candidates[MAX_PEAKS];

/*!
 * @brief Convert int16_t to Q16.16 fixed-point format.
 */
static inline int32_t to_q16(int16_t value)
{
    return ((int32_t)value) << Q16_SHIFT;
}

/*!
 * @brief Convert Q16.16 back to int16_t (with rounding).
 */
static inline int16_t from_q16(int32_t value_q16)
{
    int32_t rounded = (value_q16 + Q16_HALF) >> Q16_SHIFT;
    
    if (rounded > INT16_MAX) {
        rounded = INT16_MAX;
    } else if (rounded < INT16_MIN) {
        rounded = INT16_MIN;
    }
    
    return (int16_t)rounded;
}

/*!
 * @brief Calculate gradient at a point using central difference.
 * 
 * For interior points: grad[i] = (signal[i+1] - signal[i-1]) / 2
 * For boundaries: forward/backward difference
 *
 * @param signal_q16 Signal array
 * @param length Signal length
 * @param index Point to calculate gradient at
 * @return Gradient in Q16.16 format
 */
static int32_t compute_gradient_at(const int32_t signal_q16[], 
                                    int32_t length, 
                                    int32_t index)
{
    int32_t grad;
    
    if (index == 0) {
        /* Forward difference */
        grad = signal_q16[1] - signal_q16[0];
    } else if (index == (length - 1)) {
        /* Backward difference */
        grad = signal_q16[length - 1] - signal_q16[length - 2];
    } else {
        /* Central difference, divide by 2 */
        grad = (signal_q16[index + 1] - signal_q16[index - 1]) >> 1;
    }
    
    return grad;
}

/*!
 * @brief Calculate MATLAB-compatible topological prominence.
 *
 * True algorithm:
 * 1. Walk left until finding a higher peak or reaching boundary, track minimum
 * 2. Walk right until finding a higher peak or reaching boundary, track minimum
 * 3. Prominence = peak_value - max(left_minimum, right_minimum)
 *
 * @param signal_q16 Signal array (Q16.16)
 * @param length Signal length
 * @param peak_idx Peak index
 * @return Prominence in Q16.16 format
 */
static int32_t calculate_topological_prominence(const int32_t signal_q16[],
                                                 int32_t length,
                                                 int32_t peak_idx)
{
    int32_t peak_value = signal_q16[peak_idx];
    int32_t left_min = peak_value;
    int32_t right_min = peak_value;
    int32_t i;
    int32_t ref_level;
    
    /* Walk left contour: stop at higher peak or boundary */
    for (i = peak_idx - 1; i >= 0; i--) {
        if (signal_q16[i] >= peak_value) {
            /* Found higher or equal peak, stop here */
            break;
        }
        if (signal_q16[i] < left_min) {
            left_min = signal_q16[i];
        }
    }
    
    /* Walk right contour: stop at higher peak or boundary */
    for (i = peak_idx + 1; i < length; i++) {
        if (signal_q16[i] >= peak_value) {
            /* Found higher or equal peak, stop here */
            break;
        }
        if (signal_q16[i] < right_min) {
            right_min = signal_q16[i];
        }
    }
    
    /* Reference level is the higher of the two minima */
    ref_level = (left_min > right_min) ? left_min : right_min;
    
    return peak_value - ref_level;
}

/*!
 * @brief Find peak candidates using gradient analysis.
 *
 * Detects peaks where:
 * 1. Gradient changes from positive to negative/zero (zero-crossing)
 * 2. Point is above noise floor
 * 3. Gradient magnitude exceeds threshold
 *
 * @param signal_q16 Input signal (Q16.16)
 * @param length Signal length
 * @param config Configuration parameters
 * @param peak_indices Output array for peak indices
 * @param max_peaks Maximum number of peaks to find
 * @param num_peaks Output: number of peaks found
 * @return PEAK_FP_OK on success
 */
static PeakResultFP find_peak_candidates(const int32_t signal_q16[],
                                          int32_t length,
                                          const PeakConfigFP *config,
                                          int32_t peak_indices[],
                                          int32_t max_peaks,
                                          int32_t *num_peaks)
{
    int32_t i;
    int32_t count = 0;
    int32_t grad_prev = 0;
    int32_t grad_curr;
    
    *num_peaks = 0;
    
    if (length < 3) {
        return PEAK_FP_BUFFER_TOO_SMALL;
    }
    
    /* Compute initial gradient */
    grad_prev = compute_gradient_at(signal_q16, length, 0);
    
    /* Scan for zero-crossings in gradient */
    for (i = 1; i < (length - 1); i++) {
        grad_curr = compute_gradient_at(signal_q16, length, i);
        
        /* Check for peak conditions:
         * 1. Gradient zero-crossing: positive -> negative/zero
         * 2. OR local maximum (signal[i] > neighbors)
         * 3. Signal above noise floor
         * 4. Gradient magnitude sufficient
         */
        bool is_zero_crossing = (grad_prev > 0) && (grad_curr <= 0);
        bool is_local_max = (signal_q16[i] > signal_q16[i - 1]) && 
                            (signal_q16[i] > signal_q16[i + 1]);
        bool above_noise = (signal_q16[i] > config->noise_floor_q16);
        
        /* Check gradient magnitude: use absolute value */
        int32_t grad_mag = (grad_prev > 0) ? grad_prev : -grad_prev;
        bool strong_gradient = (grad_mag >= config->gradient_threshold_q16);
        
        if ((is_zero_crossing || is_local_max) && above_noise && strong_gradient) {
            if (count < max_peaks) {
                peak_indices[count] = i;
                count++;
            } else {
                break;  /* Peak buffer full */
            }
        }
        
        grad_prev = grad_curr;
    }
    
    *num_peaks = count;
    return PEAK_FP_OK;
}

/*!
 * @brief Select the most prominent peak from candidates.
 *
 * @param signal_q16 Signal array (Q16.16)
 * @param length Signal length
 * @param peak_indices Array of candidate peak indices
 * @param num_peaks Number of candidate peaks
 * @param config Configuration parameters
 * @param best_peak_idx Output: index of most prominent peak
 * @param best_prominence Output: prominence value (optional, can be NULL)
 * @return PEAK_FP_OK if valid peak found
 */
static PeakResultFP select_prominent_peak(const int32_t signal_q16[],
                                           int32_t length,
                                           const int32_t peak_indices[],
                                           int32_t num_peaks,
                                           const PeakConfigFP *config,
                                           int32_t *best_peak_idx,
                                           int32_t *best_prominence)
{
    int32_t i;
    int32_t max_prominence = INT32_MIN;
    int32_t best_idx = -1;
    
    /* Evaluate each candidate peak */
    for (i = 0; i < num_peaks; i++) {
        int32_t idx = peak_indices[i];
        int32_t prominence = calculate_topological_prominence(signal_q16, length, idx);
        
        /* Keep track of most prominent peak above threshold */
        if ((prominence >= config->prominence_threshold_q16) && 
            (prominence > max_prominence)) {
            max_prominence = prominence;
            best_idx = idx;
        }
    }
    
    if (best_idx >= 0) {
        *best_peak_idx = best_idx;
        if (best_prominence != NULL) {
            *best_prominence = max_prominence;
        }
        return PEAK_FP_OK;
    }
    
    return PEAK_FP_NO_PEAK_FOUND;
}

/*!
 * @brief Main entry point: Find the most prominent peak in the signal.
 *
 * Uses static buffers to minimize stack usage.
 *
 * @param signal Input signal array (int16_t ADC samples)
 * @param length Signal length (must be <= MAX_SIGNAL_LENGTH)
 * @param peak_index Output: index of detected peak
 * @param user_config Optional configuration (NULL for default)
 * @return PEAK_FP_OK if peak found, error code otherwise
 */
PeakResultFP find_prominent_peak_fp(const int16_t signal[],
                                     int32_t length,
                                     int32_t *peak_index,
                                     const PeakConfigFP *user_config)
{
    int32_t num_candidates;
    int32_t i;
    PeakResultFP result;
    const PeakConfigFP *config;
    
    /* Validate inputs */
    if ((signal == NULL) || (peak_index == NULL)) {
        return PEAK_FP_INVALID_INPUT;
    }
    
    if ((length <= 0) || (length > MAX_SIGNAL_LENGTH)) {
        return PEAK_FP_INVALID_INPUT;
    }
    
    /* Use provided config or default */
    config = (user_config != NULL) ? user_config : &default_config_fp;
    
    /* Convert input signal to Q16.16 (use static buffer) */
    for (i = 0; i < length; i++) {
        s_signal_q16[i] = to_q16(signal[i]);
    }
    
    /* Find peak candidates using gradient analysis */
    result = find_peak_candidates(s_signal_q16, length, config,
                                   s_peak_candidates, MAX_PEAKS, &num_candidates);
    if (result != PEAK_FP_OK) {
        return result;
    }
    
    if (num_candidates == 0) {
        return PEAK_FP_NO_PEAK_FOUND;
    }
    
    /* Select most prominent peak using topological prominence */
    result = select_prominent_peak(s_signal_q16, length, s_peak_candidates,
                                    num_candidates, config, peak_index, NULL);
    
    return result;
}

/*!
 * @brief Alternative API with caller-provided buffers (for thread-safety).
 *
 * Allows caller to provide their own buffers, avoiding static storage.
 * Useful for multi-threaded environments or when static buffers are in use.
 *
 * @param signal Input signal array
 * @param length Signal length
 * @param peak_index Output: index of detected peak
 * @param user_config Optional configuration
 * @param signal_q16_buffer Caller-provided buffer (length elements)
 * @param peaks_buffer Caller-provided buffer (MAX_PEAKS elements)
 * @return PEAK_FP_OK if peak found
 */
PeakResultFP find_prominent_peak_fp_buffered(const int16_t signal[],
                                              int32_t length,
                                              int32_t *peak_index,
                                              const PeakConfigFP *user_config,
                                              int32_t *signal_q16_buffer,
                                              int32_t *peaks_buffer)
{
    int32_t num_candidates;
    int32_t i;
    PeakResultFP result;
    const PeakConfigFP *config;
    
    /* Validate inputs */
    if ((signal == NULL) || (peak_index == NULL) || 
        (signal_q16_buffer == NULL) || (peaks_buffer == NULL)) {
        return PEAK_FP_INVALID_INPUT;
    }
    
    if ((length <= 0) || (length > MAX_SIGNAL_LENGTH)) {
        return PEAK_FP_INVALID_INPUT;
    }
    
    config = (user_config != NULL) ? user_config : &default_config_fp;
    
    /* Convert input signal to Q16.16 */
    for (i = 0; i < length; i++) {
        signal_q16_buffer[i] = to_q16(signal[i]);
    }
    
    /* Find peak candidates */
    result = find_peak_candidates(signal_q16_buffer, length, config,
                                   peaks_buffer, MAX_PEAKS, &num_candidates);
    if (result != PEAK_FP_OK) {
        return result;
    }
    
    if (num_candidates == 0) {
        return PEAK_FP_NO_PEAK_FOUND;
    }
    
    /* Select most prominent peak */
    result = select_prominent_peak(signal_q16_buffer, length, peaks_buffer,
                                    num_candidates, config, peak_index, NULL);
    
    return result;
}

/*!
 * @brief Helper: Get peak prominence (for debugging/validation).
 *
 * @param signal Input signal
 * @param length Signal length
 * @param peak_index Peak index
 * @return Prominence as float
 */
float get_peak_prominence_float(const int16_t signal[], 
                                 int32_t length,
                                 int32_t peak_index)
{
    int32_t prominence_q16;
    int32_t i;
    
    if ((length <= 0) || (length > MAX_SIGNAL_LENGTH) || 
        (peak_index < 0) || (peak_index >= length)) {
        return 0.0f;
    }
    
    /* Convert to Q16.16 */
    for (i = 0; i < length; i++) {
        s_signal_q16[i] = to_q16(signal[i]);
    }
    
    prominence_q16 = calculate_topological_prominence(s_signal_q16, length, peak_index);
    
    return (float)prominence_q16 / (float)Q16_ONE;
}
