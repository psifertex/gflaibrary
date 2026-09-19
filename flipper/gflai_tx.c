#include "gflai_tx.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

#define TAG "GflaiTx"

/* PWM symbol timings, in microseconds, averaged over captures/01_red.sub. */
#define PULSE_SHORT 200u
#define PULSE_LONG  600u

/* Every burst ends with a short mark and then a gap of one of these lengths. */
#define GAP_FRAME 1600u /* preamble -> payload, and payload -> payload */
#define GAP_BLOCK 5600u /* after the final payload of a block */

#define PAYLOAD_BYTES   7
#define PAYLOAD_REPEATS 2

/* Two LevelDuration entries per bit, plus two for the trailing mark and gap. */
#define ENTRIES_PREAMBLE ((3 * 2) + 2)
#define ENTRIES_PAYLOAD  ((PAYLOAD_BYTES * 8 * 2) + 2)
#define ENTRIES_MAX      (ENTRIES_PREAMBLE + (PAYLOAD_REPEATS * ENTRIES_PAYLOAD))

struct GflaiTx {
    const SubGhzDevice* device;
    bool open;
    size_t len;
    size_t pos;
    LevelDuration buf[ENTRIES_MAX];
};

uint8_t gflai_checksum(uint8_t r, uint8_t g, uint8_t b) {
    return (uint8_t)(((r ^ g ^ b ^ GFLAI_CK_NIBBLE) << 4) | GFLAI_CK_NIBBLE);
}

static void push(GflaiTx* tx, bool level, uint32_t duration) {
    furi_check(tx->len < ENTRIES_MAX);
    tx->buf[tx->len++] = level_duration_make(level, duration);
}

static void push_bit(GflaiTx* tx, bool one) {
    push(tx, true, one ? PULSE_SHORT : PULSE_LONG);
    push(tx, false, one ? PULSE_LONG : PULSE_SHORT);
}

static void push_byte(GflaiTx* tx, uint8_t value) {
    for(int i = 7; i >= 0; i--) push_bit(tx, (value >> i) & 1u);
}

/* A burst is terminated by a lone short mark followed by a long silence. */
static void push_stop(GflaiTx* tx, uint32_t gap) {
    push(tx, true, PULSE_SHORT);
    push(tx, false, gap);
}

static void gflai_build(GflaiTx* tx, uint8_t r, uint8_t g, uint8_t b) {
    tx->len = 0;
    tx->pos = 0;

    /* Preamble is the nibble 0101; the stop mark supplies that trailing 1. */
    push_bit(tx, false);
    push_bit(tx, true);
    push_bit(tx, false);
    push_stop(tx, GAP_FRAME);

    for(int rep = 0; rep < PAYLOAD_REPEATS; rep++) {
        push_byte(tx, 0xAA);
        push_byte(tx, 0xFF);
        push_byte(tx, (uint8_t)(r << 4));
        push_byte(tx, (uint8_t)(g << 4));
        push_byte(tx, (uint8_t)(b << 4));
        push_byte(tx, gflai_checksum(r, g, b));
        push_byte(tx, 0x00);
        push_stop(tx, (rep == PAYLOAD_REPEATS - 1) ? GAP_BLOCK : GAP_FRAME);
    }
}

static LevelDuration gflai_tx_yield(void* context) {
    GflaiTx* tx = context;
    if(tx->pos >= tx->len) return level_duration_reset();
    return tx->buf[tx->pos++];
}

GflaiTx* gflai_tx_alloc(void) {
    return malloc(sizeof(GflaiTx));
}

void gflai_tx_free(GflaiTx* tx) {
    furi_assert(tx);
    if(tx->open) gflai_tx_close(tx);
    free(tx);
}

GflaiTxStatus gflai_tx_open(GflaiTx* tx) {
    furi_assert(tx);
    tx->open = false;
    tx->len = 0;
    tx->pos = 0;

    subghz_devices_init();
    tx->device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    if(!tx->device) {
        subghz_devices_deinit();
        return GflaiTxErrNoDevice;
    }

    /* The internal radio leaves .begin NULL and so always reports false here;
     * only external modules use it to power up.  is_connect is the real probe.
     */
    subghz_devices_begin(tx->device);
    if(!subghz_devices_is_connect(tx->device)) {
        subghz_devices_end(tx->device);
        subghz_devices_deinit();
        return GflaiTxErrBegin;
    }

    subghz_devices_reset(tx->device);
    subghz_devices_idle(tx->device);
    subghz_devices_load_preset(tx->device, FuriHalSubGhzPresetOok650Async, NULL);
    subghz_devices_set_frequency(tx->device, GFLAI_FREQUENCY);

    if(subghz_devices_check_tx(tx->device, GFLAI_FREQUENCY) != SubGhzTxAllowed) {
        subghz_devices_sleep(tx->device);
        subghz_devices_end(tx->device);
        subghz_devices_deinit();
        return GflaiTxErrRegion;
    }

    /* Charging noise sits right on top of the band. */
    furi_hal_power_suppress_charge_enter();
    tx->open = true;
    return GflaiTxOk;
}

void gflai_tx_close(GflaiTx* tx) {
    furi_assert(tx);
    if(!tx->open) return;
    subghz_devices_idle(tx->device);
    subghz_devices_sleep(tx->device);
    subghz_devices_end(tx->device);
    subghz_devices_deinit();
    furi_hal_power_suppress_charge_exit();
    tx->open = false;
}

const char* gflai_tx_status_text(GflaiTxStatus status) {
    switch(status) {
    case GflaiTxOk:
        return "OK";
    case GflaiTxErrNoDevice:
        return "No radio found";
    case GflaiTxErrBegin:
        return "Radio not responding";
    case GflaiTxErrRegion:
        return "433.92 blocked\nby region";
    default:
        return "Unknown error";
    }
}

bool gflai_tx_send(GflaiTx* tx, uint8_t r, uint8_t g, uint8_t b) {
    furi_assert(tx);
    if(!tx->open) return false;

    gflai_build(tx, r, g, b);

    if(!subghz_devices_start_async_tx(tx->device, gflai_tx_yield, tx)) {
        FURI_LOG_E(TAG, "async tx refused");
        return false;
    }

    while(!subghz_devices_is_async_complete_tx(tx->device)) {
        furi_delay_ms(2);
    }
    subghz_devices_stop_async_tx(tx->device);
    subghz_devices_idle(tx->device);
    return true;
}
