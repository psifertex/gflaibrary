/* Builds one on-air block on the host and prints it as Flipper RAW_Data pairs,
 * so test_encoder.py can diff it against the captures.  Pulls in the real
 * encoder rather than a copy of it, statics and all.
 */
#include "../gflai_tx.c"

#define MAX_RECORDED 4096

static LevelDuration recorded[MAX_RECORDED];
static size_t recorded_count;

void subghz_devices_init(void) {
}

void subghz_devices_deinit(void) {
}

const SubGhzDevice* subghz_devices_get_by_name(const char* device_name) {
    (void)device_name;
    return (const SubGhzDevice*)&recorded; /* any non-NULL handle */
}

/* Matches the internal radio, which leaves .begin NULL. */
bool subghz_devices_begin(const SubGhzDevice* device) {
    (void)device;
    return false;
}

void subghz_devices_end(const SubGhzDevice* device) {
    (void)device;
}

bool subghz_devices_is_connect(const SubGhzDevice* device) {
    (void)device;
    return true;
}

void subghz_devices_reset(const SubGhzDevice* device) {
    (void)device;
}

void subghz_devices_sleep(const SubGhzDevice* device) {
    (void)device;
}

void subghz_devices_idle(const SubGhzDevice* device) {
    (void)device;
}

void subghz_devices_load_preset(
    const SubGhzDevice* device,
    FuriHalSubGhzPreset preset,
    uint8_t* preset_data) {
    (void)device;
    (void)preset;
    (void)preset_data;
}

uint32_t subghz_devices_set_frequency(const SubGhzDevice* device, uint32_t frequency) {
    (void)device;
    return frequency;
}

SubGhzTx subghz_devices_check_tx(const SubGhzDevice* device, uint32_t frequency) {
    (void)device;
    (void)frequency;
    return SubGhzTxAllowed;
}

bool subghz_devices_start_async_tx(const SubGhzDevice* device, void* callback, void* context) {
    (void)device;
    LevelDuration (*yield)(void*) = callback;
    recorded_count = 0;
    while(recorded_count < MAX_RECORDED) {
        LevelDuration ld = yield(context);
        if(level_duration_is_reset(ld)) break;
        recorded[recorded_count++] = ld;
    }
    return true;
}

bool subghz_devices_is_async_complete_tx(const SubGhzDevice* device) {
    (void)device;
    return true;
}

void subghz_devices_stop_async_tx(const SubGhzDevice* device) {
    (void)device;
}

int main(int argc, char** argv) {
    if(argc != 4) {
        fprintf(stderr, "usage: %s <r> <g> <b>   (each 0-15)\n", argv[0]);
        return 2;
    }

    uint8_t r = (uint8_t)atoi(argv[1]);
    uint8_t g = (uint8_t)atoi(argv[2]);
    uint8_t b = (uint8_t)atoi(argv[3]);

    GflaiTx* tx = gflai_tx_alloc();
    if(gflai_tx_open(tx) != GflaiTxOk) {
        fprintf(stderr, "open failed\n");
        return 1;
    }
    if(!gflai_tx_send(tx, r, g, b)) {
        fprintf(stderr, "send failed\n");
        return 1;
    }
    gflai_tx_free(tx);

    printf("# checksum %02x\n", gflai_checksum(r, g, b));
    for(size_t i = 0; i < recorded_count; i++) {
        uint32_t duration = level_duration_get_duration(recorded[i]);
        printf("%s%u\n", level_duration_get_level(recorded[i]) ? "" : "-", duration);
    }
    return 0;
}
