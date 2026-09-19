#pragma once

#include <stdbool.h>
#include <stdint.h>

/* GFLAI wristband radio link.
 *
 * Timings below are measured from the ShmooCon captures in ../captures, not
 * copied from generate.py: the generator pads the preamble out to a full 0x50
 * byte, while the wire only ever carries the nibble.  See ../README.md.
 */
#define GFLAI_FREQUENCY 433920000u

/* The receivers ignore anything but a 5 here; every other value was dropped. */
#define GFLAI_CK_NIBBLE 5u

/* One block on air, preamble plus two payload repeats. */
#define GFLAI_BLOCK_MS 102u

typedef struct GflaiTx GflaiTx;

typedef enum {
    GflaiTxOk,
    GflaiTxErrNoDevice,
    GflaiTxErrBegin,
    GflaiTxErrRegion,
} GflaiTxStatus;

GflaiTx* gflai_tx_alloc(void);
void gflai_tx_free(GflaiTx* tx);

/* Claims the radio; must be paired with gflai_tx_close(). */
GflaiTxStatus gflai_tx_open(GflaiTx* tx);
void gflai_tx_close(GflaiTx* tx);

const char* gflai_tx_status_text(GflaiTxStatus status);

/* Each channel is a 0-15 level; the wristbands only decode the upper nibble. */
bool gflai_tx_send(GflaiTx* tx, uint8_t r, uint8_t g, uint8_t b);

uint8_t gflai_checksum(uint8_t r, uint8_t g, uint8_t b);
