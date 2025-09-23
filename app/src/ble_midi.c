#include "ble_midi.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(ble_midi, CONFIG_APP_LOG_LEVEL);

static void (*ble_midi_started_cb)(void);
static bool ble_midi_started = false;

static struct pots_params params;
static bool params_changed = false;

static void htmc_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                 uint16_t value) {
    uint8_t notification_enabled = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
    LOG_DBG("MIDI Notifications %s",
            notification_enabled ? "enabled" : "disabled");

    if (!notification_enabled) {
        ble_midi_started = false;
    }
}

ssize_t midi_read_char(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                       void *buf, uint16_t len, uint16_t offset) {
    if (!ble_midi_started) {
        ble_midi_started = true;
        if (ble_midi_started_cb) ble_midi_started_cb();
        LOG_DBG("MIDI started");
    }

    return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t midi_write_char(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
    if (len >= 8) {
        params.minimum_change = sys_get_le16(buf);
        params.slow_refresh_period_ms = sys_get_le16((uint8_t *)buf + 2);
        params.fast_refresh_period_ms = sys_get_le16((uint8_t *)buf + 4);
        params.fast_refresh_retention_ms = sys_get_le16((uint8_t *)buf + 6);

        params_changed = true;
    }
    return len;
}

BT_GATT_SERVICE_DEFINE(midi_ble_svc,
                       BT_GATT_PRIMARY_SERVICE(BT_UUID_MIDI),
                       BT_GATT_CHARACTERISTIC(BT_UUID_MIDI_CHAR, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, midi_read_char, midi_write_char, NULL),
                       BT_GATT_CCC(htmc_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

void ble_midi_init(void (*ble_midi_started_cb_)(void)) {
    ble_midi_started_cb = ble_midi_started_cb_;
}

bool ble_midi_is_started(void) {
    return ble_midi_started;
}

bool ble_midi_params_changed(void) {
    return params_changed;
}

void ble_midi_get_params(struct pots_params *out_params) {
    if (out_params) {
        memcpy(out_params, &params, sizeof(params));
    }
    params_changed = false;
}

int ble_midi_send_packet(const uint8_t *data, size_t len) {
    if (!ble_midi_started) {
        return -EACCES;
    }

    return bt_gatt_notify(NULL, &midi_ble_svc.attrs[1], data, len);
}