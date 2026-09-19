#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A scene is either a fixed colour or a function of the frame counter.
 * step_ms is the desired spacing between frames; a block takes about
 * GFLAI_BLOCK_MS on air, so anything at or below that runs flat out.
 */
typedef void (*GflaiFrameFn)(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b);

typedef struct {
    const char* name;
    uint8_t r, g, b; /* used when frame is NULL */
    GflaiFrameFn frame;
    uint16_t step_ms;
} GflaiScene;

extern const GflaiScene gflai_scenes[];
extern const uint8_t gflai_scene_count;

static inline bool gflai_scene_is_pattern(const GflaiScene* scene) {
    return scene->frame != NULL;
}

/* Resolves a scene to a colour and applies the 0-15 master brightness. */
void gflai_scene_render(
    const GflaiScene* scene,
    uint32_t tick,
    uint8_t brightness,
    uint8_t* r,
    uint8_t* g,
    uint8_t* b);
