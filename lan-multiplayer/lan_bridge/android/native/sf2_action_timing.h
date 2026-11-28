#ifndef SF2_ACTION_TIMING_H
#define SF2_ACTION_TIMING_H
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

// Only owner-clock deltas are accepted. Fall back unchanged if freshness,
// action identity, relative-start semantics or sample validity are uncertain.
static inline int32_t lan_action_start_frame(int32_t original, int32_t initial,
    int32_t period, float owner_age, int64_t freshness_ms, bool eligible) {
    if (!eligible || initial < 0 || initial > 100000 || period != 3 ||
        freshness_ms < 0 || freshness_ms > 250 || !isfinite(owner_age) ||
        owner_age < period || owner_age > 120.0f) return original;
    return initial + (int32_t)(owner_age / period);
}
#endif
