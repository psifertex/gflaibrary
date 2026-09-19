#include "gflai_scenes.h"

#include <stddef.h>

/* Colours start from the cyberputer renderer palette (firmware/src/core/
 * visualization/renderer.cpp), reduced to the 4 bits per channel the
 * wristbands decode, and then saturated.
 *
 * That palette is built for an emissive screen on a near-black background,
 * where the surrounding black supplies the contrast: every accent has all
 * three channels lit, e.g. violet 0xb080ff is (11,8,15).  A wristband LED has
 * no dark surround, so the same values just read as washed out.  Pulling the
 * white floor out while keeping the hue and the peak level gives the colour
 * the palette was reaching for.  Near-greys like white are left alone, since
 * saturating them has no meaningful hue to land on.
 */
#define C_VIOLET 6, 0, 15 /* 0xb080ff, was 11,8,15 */
#define C_CYAN   0, 15, 13 /* 0x65ffe0, was 6,15,14 */
#define C_PINK   15, 0, 10 /* 0xff59cc, was 15,5,12 */
#define C_AMBER  15, 9, 0 /* 0xffca73, was 15,12,7 */
#define C_WHITE  14, 15, 15 /* 0xe4fff9, near-grey, unchanged */
#define C_STEEL  0, 2, 5 /* 0x31405a "rain", was 3,4,5 */
#define C_OFF    0, 0, 0

static const uint8_t VIOLET[3] = {C_VIOLET};
static const uint8_t CYAN[3] = {C_CYAN};
static const uint8_t AMBER[3] = {C_AMBER};
static const uint8_t STEEL[3] = {C_STEEL};

/* One period of a sine, quantised to the 16 levels the hardware has. */
static const uint8_t SIN15[64] = {8,  8,  9,  10, 10, 11, 12, 12, 13, 13, 14, 14, 14,
                                  15, 15, 15, 15, 15, 15, 15, 14, 14, 14, 13, 13, 12,
                                  12, 11, 10, 10, 9,  8,  8,  7,  6,  5,  5,  4,  3,
                                  3,  2,  2,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,
                                  1,  1,  1,  2,  2,  3,  3,  4,  5,  5,  6,  7};

/* Deterministic per-frame noise, so a scene stays a pure function of tick. */
static uint32_t hash32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static uint8_t scale(uint8_t value, uint8_t numerator, uint8_t denominator) {
    return (uint8_t)(((uint16_t)value * numerator) / denominator);
}

/* Walks a ring of key colours, interpolating steps frames between each pair. */
static void fade_through(
    const uint8_t keys[][3],
    uint8_t count,
    uint32_t tick,
    uint8_t steps,
    uint8_t* r,
    uint8_t* g,
    uint8_t* b) {
    uint32_t position = tick % ((uint32_t)count * steps);
    uint8_t from = (uint8_t)(position / steps);
    uint8_t to = (uint8_t)((from + 1) % count);
    int32_t f = (int32_t)(position % steps);

    uint8_t* out[3] = {r, g, b};
    for(int i = 0; i < 3; i++) {
        int32_t a = keys[from][i];
        int32_t z = keys[to][i];
        *out[i] = (uint8_t)(a + ((z - a) * f) / steps);
    }
}

/* Neon signs cycling across the skyline. */
static const uint8_t CITY_KEYS[4][3] = {{C_VIOLET}, {C_PINK}, {C_AMBER}, {C_CYAN}};

static void scene_city(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b) {
    fade_through(CITY_KEYS, 4, tick, 12, r, g, b);
}

/* Three detuned sines, the closest the band can get to the plasma scene. */
static void scene_odyssey(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = SIN15[tick & 63];
    *g = SIN15[(((tick * 3) / 2) + 21) & 63];
    *b = SIN15[(((tick * 5) / 4) + 43) & 63];
    /* Keep a floor so the band never looks like it dropped out. */
    if(*r + *g + *b < 2) *b = 2;
}

/* Cool end of the palette only, so it stays calmer than Neon City. */
static const uint8_t DRIFT_KEYS[4][3] = {{6, 0, 15}, {0, 4, 15}, {C_CYAN}, {3, 0, 10}};

/* The wear-it-all-evening scene: a 32s hue drift under a 17s swell, the two
 * periods deliberately unrelated so the loop never announces itself.
 */
static void scene_drift(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b) {
    fade_through(DRIFT_KEYS, 4, tick, 40, r, g, b);

    uint8_t envelope = (uint8_t)(6 + (SIN15[((tick * 3) / 4) & 63] * 9) / 15);
    *r = scale(*r, envelope, 15);
    *g = scale(*g, envelope, 15);
    *b = scale(*b, envelope, 15);
}

/* Amber ping, then a decaying cyan trail behind the sweep line. */
static void scene_radar(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b) {
    uint32_t phase = tick % 16;
    if(phase == 0) {
        *r = AMBER[0];
        *g = AMBER[1];
        *b = AMBER[2];
        return;
    }
    uint8_t trail = (uint8_t)(16 - phase);
    *r = scale(CYAN[0], trail, 15);
    *g = scale(CYAN[1], trail, 15);
    *b = scale(CYAN[2], trail, 15);
}

/* Steel drizzle with the occasional bright glyph falling through it. */
static void scene_rain(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b) {
    if((hash32(tick) & 7u) == 0u) {
        *r = CYAN[0];
        *g = CYAN[1];
        *b = CYAN[2];
    } else if((hash32(tick - 1) & 7u) == 0u) {
        *r = scale(CYAN[0], 7, 15);
        *g = scale(CYAN[1], 7, 15);
        *b = scale(CYAN[2], 7, 15);
    } else {
        *r = STEEL[0];
        *g = STEEL[1];
        *b = STEEL[2];
    }
}

/* Violet hold, punched through by corrupted frames. */
static void scene_glitch(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b) {
    switch(hash32(tick) & 15u) {
    case 0:
        *r = 14;
        *g = 15;
        *b = 15;
        break;
    case 1:
        *r = 15;
        *g = 0;
        *b = 10;
        break;
    case 2:
        *r = 0;
        *g = 0;
        *b = 0;
        break;
    default:
        *r = VIOLET[0];
        *g = VIOLET[1];
        *b = VIOLET[2];
        break;
    }
}

/* 5Hz hard strobe, the fastest the protocol allows. */
static void scene_strobe(uint32_t tick, uint8_t* r, uint8_t* g, uint8_t* b) {
    uint8_t on = (tick & 1u) ? 15 : 0;
    *r = scale(14, on, 15);
    *g = on;
    *b = on;
}

#define PATTERN(label, fn, ms) {label, 0, 0, 0, fn, ms}
#define SOLID(label, colour)   {label, colour, NULL, 500}

/* Deliberately short.  Dim solids are omitted because the brightness control
 * already covers them, and scenes that felt like variations on a neighbour
 * were dropped rather than kept for the sake of a longer list.
 */
const GflaiScene gflai_scenes[] = {
    PATTERN("Neon City", scene_city, 100),
    PATTERN("Neon Odyssey", scene_odyssey, 100),
    PATTERN("Night Drift", scene_drift, 200),
    PATTERN("Signal Rain", scene_rain, 100),
    PATTERN("Neon Radar", scene_radar, 100),
    PATTERN("Glitch", scene_glitch, 100),
    PATTERN("Strobe", scene_strobe, 100),
    SOLID("Violet", C_VIOLET),
    SOLID("Cyan", C_CYAN),
    SOLID("Pink", C_PINK),
    SOLID("Amber", C_AMBER),
    SOLID("White", C_WHITE),
    SOLID("Blackout", C_OFF),
};

const uint8_t gflai_scene_count = sizeof(gflai_scenes) / sizeof(gflai_scenes[0]);

void gflai_scene_render(
    const GflaiScene* scene,
    uint32_t tick,
    uint8_t brightness,
    uint8_t* r,
    uint8_t* g,
    uint8_t* b) {
    if(scene->frame) {
        scene->frame(tick, r, g, b);
    } else {
        *r = scene->r;
        *g = scene->g;
        *b = scene->b;
    }

    if(brightness >= 15) return;
    *r = scale(*r, brightness, 15);
    *g = scale(*g, brightness, 15);
    *b = scale(*b, brightness, 15);
}
