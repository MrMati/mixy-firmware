#include <zephyr/settings/settings.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include "nrf.h"

int set_sn_from_nonvol(void) {
    char serial[5];
    uint32_t raw = NRF_UICR->CUSTOMER[0];
    memcpy(serial, &raw, 4);
    serial[4] = '\0';
    return settings_runtime_set("bt/dis/serial", serial, 5);
}

SYS_INIT(set_sn_from_nonvol, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);