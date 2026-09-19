/* Minimal host stand-ins for the firmware headers gflai_tx.c pulls in, so the
 * encoder can be compiled and checked against the captures on a desktop.
 */
#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define furi_check(expression)  assert(expression)
#define furi_assert(expression) assert(expression)

#define FURI_LOG_E(tag, ...)                 \
    do {                                     \
        fprintf(stderr, "[E][%s] ", tag);    \
        fprintf(stderr, __VA_ARGS__);        \
        fputc('\n', stderr);                 \
    } while(0)

static inline void furi_delay_ms(uint32_t ms) {
    (void)ms;
}

/* Layout copied from toolbox/level_duration.h (LEVEL_DURATION_BIG). */
#define LEVEL_DURATION_RESET      0U
#define LEVEL_DURATION_LEVEL_LOW  1U
#define LEVEL_DURATION_LEVEL_HIGH 2U

typedef struct {
    uint32_t duration : 30;
    uint8_t level     : 2;
} LevelDuration;

static inline LevelDuration level_duration_make(bool level, uint32_t duration) {
    LevelDuration ld;
    ld.level = level ? LEVEL_DURATION_LEVEL_HIGH : LEVEL_DURATION_LEVEL_LOW;
    ld.duration = duration;
    return ld;
}

static inline LevelDuration level_duration_reset(void) {
    LevelDuration ld;
    ld.level = LEVEL_DURATION_RESET;
    ld.duration = 0;
    return ld;
}

static inline bool level_duration_is_reset(LevelDuration ld) {
    return ld.level == LEVEL_DURATION_RESET;
}

static inline bool level_duration_get_level(LevelDuration ld) {
    return ld.level == LEVEL_DURATION_LEVEL_HIGH;
}

static inline uint32_t level_duration_get_duration(LevelDuration ld) {
    return ld.duration;
}
