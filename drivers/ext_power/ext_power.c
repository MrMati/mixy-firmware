#define DT_DRV_COMPAT mixy_ext_power

#include <app/drivers/ext_power.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ext_power, CONFIG_EXT_POWER_LOG_LEVEL);

struct ext_power_data {
    int current_state;
};

struct ext_power_config {
    struct gpio_dt_spec ctrl_pin;
};

static int set_state(const struct device *dev, int state) {
    const struct ext_power_config *config = dev->config;
    struct ext_power_data *data = dev->data;

    if (state == data->current_state) return 0;

    gpio_pin_set(config->ctrl_pin.port, config->ctrl_pin.pin, state != 0);

    k_busy_wait(5);

    data->current_state = state;

    return 0;
}

static DEVICE_API(ext_power, ext_power_api) = {
    .set_state = &set_state,
};

static int ext_power_init(const struct device *dev) {
    const struct ext_power_config *config = dev->config;
    int ret;

    if (!gpio_is_ready_dt(&config->ctrl_pin)) {
        LOG_ERR("Control GPIO not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&config->ctrl_pin, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Could not configure control GPIO (%d)", ret);
        return ret;
    }

    return 0;
}

#define EXT_POWER_DEFINE(inst)                                         \
    static struct ext_power_data data##inst;                           \
                                                                       \
    static const struct ext_power_config config##inst = {              \
        .ctrl_pin = GPIO_DT_SPEC_INST_GET(inst, control_gpios),        \
    };                                                                 \
                                                                       \
    DEVICE_DT_INST_DEFINE(inst, ext_power_init, NULL, &data##inst,     \
                          &config##inst, POST_KERNEL,                  \
                          CONFIG_EXT_POWER_INIT_PRIORITY,              \
                          &ext_power_api);

DT_INST_FOREACH_STATUS_OKAY(EXT_POWER_DEFINE)
