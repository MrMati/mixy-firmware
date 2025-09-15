#include <app/drivers/pots.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>

#include "ble_midi.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

static void ble_midi_started(void);
static void pots_data_task(struct k_work *work);
static void bas_notify_task(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(data_out_work, pots_data_task);
static K_WORK_DELAYABLE_DEFINE(battery_update_work, bas_notify_task);

/*     BLUETOOTH    */

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
    /*BT_DATA_BYTES(BT_DATA_UUID16_ALL,
                  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL)),*/
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_MIDI_VAL),
};

/*static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};*/

#define BT_LE_ADV_PARAMS BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN, 0x960, 0xC80, NULL)  // 1.5s to 2s interval

static bool bt_connected = false;

static void connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
    } else {
        LOG_INF("Connected");
        bt_connected = true;
        k_work_schedule(&battery_update_work, K_NO_WAIT);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    LOG_INF("Disconnected, reason 0x%02x %s", reason, bt_hci_err_to_str(reason));
    bt_connected = false;
}

static void bt_recycled() {
    LOG_DBG("Connection recycled");
    int err = bt_le_adv_start(BT_LE_ADV_PARAMS, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return;
    }

    LOG_DBG("Advertising restarted");
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .recycled = bt_recycled};

static void bt_ready(void) {
    LOG_INF("Bluetooth initialized");

    ble_midi_init(ble_midi_started);

    int err = bt_le_adv_start(BT_LE_ADV_PARAMS, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return;
    }

    LOG_INF("Advertising started");
}

/*     BATTERY    */
static const struct device *const battery = DEVICE_DT_GET(DT_CHOSEN(mixy_battery));

static void bas_notify_task(struct k_work *work) {
    struct sensor_value state_of_charge;
    int ret;

    ret = sensor_sample_fetch_chan(battery, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE);
    if (ret != 0) {
        LOG_DBG("Failed to fetch battery values: %d", ret);
        return;
    }

    ret = sensor_channel_get(battery, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &state_of_charge);
    if (ret != 0) {
        LOG_DBG("Failed to get battery state of charge: %d", ret);
        return;
    }

    ret = bt_bas_set_battery_level(state_of_charge.val1);
    if (ret) {
        LOG_DBG("Failed to update battery level: %d", ret);
        // keep trying
    }

    if (bt_connected) {
        k_work_schedule(&battery_update_work, K_SECONDS(60));
    }
}

/*     APP     */

static void make_midi_packet(uint8_t *packet, uint16_t timestamp, uint8_t status, uint8_t data1, uint8_t data2) {
    uint8_t ts = 0x80 | (timestamp & 0x7F);
    uint8_t header = 0x80 | ((timestamp >> 7) & 0x3F);
    packet[0] = header;
    packet[1] = ts;
    packet[2] = status;
    packet[3] = data1;
    packet[4] = data2;
}

static uint8_t pot_val_norm(uint16_t raw_val) {
    uint8_t value_norm = (uint8_t)((127 * raw_val) / 930);
    if (value_norm >= 127) value_norm = 127;
    return value_norm;
}

static uint8_t pot_idx_mapping[6] = {4, 2, 0, 5, 3, 1};

static void send_pot_val(uint8_t pot_idx, uint16_t curr) {
    uint8_t packet[5];
    // with current ADC config
    // 3.333 / (0.6/(1/6) )* 1023 == 947 max

    pot_idx = pot_idx_mapping[pot_idx];

    // MIDI Control Change: status = 0xB0 | channel
    make_midi_packet(packet, 0, 0xB0, pot_idx, pot_val_norm(curr));
    ble_midi_send_packet(packet, sizeof(packet));
}

#define POTS_AMOUNT 6

static const struct device *pots;
static uint16_t prev_pot_vals[POTS_AMOUNT];
static int64_t last_change_time;

static struct pots_params params;

static void ble_midi_started(void) {
    k_work_schedule(&data_out_work, K_NO_WAIT);
}

static void pots_data_task(struct k_work *work) {
    if (ble_midi_params_changed()) {
        ble_midi_get_params(&params);
    }

    uint16_t curr_pot_vals[POTS_AMOUNT];
    mixy_pots_read(pots, curr_pot_vals);

    bool changed = false;

    for (uint8_t i = 0; i < POTS_AMOUNT; i++) {
        if (i == 3) continue;  // ignore pot 3 as it is NC
        if (abs(curr_pot_vals[i] - prev_pot_vals[i]) > params.minimum_change) {
            send_pot_val(i, curr_pot_vals[i]);
            prev_pot_vals[i] = curr_pot_vals[i];
            changed = true;
        }
    }

    if (changed) {
        last_change_time = k_uptime_get();
    }

    if (!ble_midi_is_started()) return;

    if (k_uptime_get() - last_change_time > params.fast_refresh_retention_ms) {
        k_work_schedule(&data_out_work, K_MSEC(params.slow_refresh_period_ms));
    } else {
        k_work_schedule(&data_out_work, K_MSEC(params.fast_refresh_period_ms));
    }
}

int main(void) {
    int ret;

    pots = DEVICE_DT_GET(DT_NODELABEL(pots));
    if (!device_is_ready(pots)) {
        LOG_ERR("Pots not ready");
        return 0;
    }

    params.minimum_change = 10;
    params.slow_refresh_period_ms = 500;
    params.fast_refresh_period_ms = 80;
    params.fast_refresh_retention_ms = 1000;

    ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("Bluetooth init failed (err %d)", ret);
        return 0;
    }
    bt_ready();

    LOG_INF("Mixy init done");

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
