#pragma once

#include <furi.h>

typedef enum {
    FuriHalSubGhzPresetIDLE,
    FuriHalSubGhzPresetOok270Async,
    FuriHalSubGhzPresetOok650Async,
} FuriHalSubGhzPreset;

typedef enum {
    SubGhzTxAllowed,
    SubGhzTxBlockedRegionNotProvisioned,
    SubGhzTxBlockedRegion,
    SubGhzTxBlockedDefault,
    SubGhzTxUnsupported,
} SubGhzTx;

typedef struct SubGhzDevice SubGhzDevice;

void subghz_devices_init(void);
void subghz_devices_deinit(void);
const SubGhzDevice* subghz_devices_get_by_name(const char* device_name);
bool subghz_devices_begin(const SubGhzDevice* device);
void subghz_devices_end(const SubGhzDevice* device);
bool subghz_devices_is_connect(const SubGhzDevice* device);
void subghz_devices_reset(const SubGhzDevice* device);
void subghz_devices_sleep(const SubGhzDevice* device);
void subghz_devices_idle(const SubGhzDevice* device);
void subghz_devices_load_preset(
    const SubGhzDevice* device,
    FuriHalSubGhzPreset preset,
    uint8_t* preset_data);
uint32_t subghz_devices_set_frequency(const SubGhzDevice* device, uint32_t frequency);
bool subghz_devices_start_async_tx(const SubGhzDevice* device, void* callback, void* context);
bool subghz_devices_is_async_complete_tx(const SubGhzDevice* device);
void subghz_devices_stop_async_tx(const SubGhzDevice* device);
SubGhzTx subghz_devices_check_tx(const SubGhzDevice* device, uint32_t frequency);
