#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_USE_REAL_MIDI_UUID
#define BT_UUID_MIDI_VAL BT_UUID_128_ENCODE(0x03B80E5A, 0xEDE8, 0x4B33, 0xA751, 0x6CE34EC4C700)
#else
#define BT_UUID_MIDI_VAL BT_UUID_128_ENCODE(0x03B80E5A, 0xEDE8, 0x4B33, 0xA751, 0x6CE34EC4C705)
#endif

#define BT_UUID_MIDI BT_UUID_DECLARE_128(BT_UUID_MIDI_VAL)
#define BT_UUID_MIDI_CHAR BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x7772E5DB, 0x3868, 0x4112, 0xA1A9, 0xF2669D106BF3))

struct pots_params {
    uint16_t minimum_change;
    uint16_t slow_refresh_period_ms;
    uint16_t fast_refresh_period_ms;
    uint16_t fast_refresh_retention_ms;
};

void ble_midi_init(void (*ble_midi_started_cb)(void));
bool ble_midi_is_started(void);
bool ble_midi_params_changed(void);
void ble_midi_get_params(struct pots_params *out_params);
int ble_midi_send_packet(const uint8_t *data, size_t len);
