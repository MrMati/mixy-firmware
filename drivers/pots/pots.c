#define DT_DRV_COMPAT mixy_pots

#include <app/drivers/ext_power.h>
#include <app/drivers/pots.h>
#include <nrfx_saadc.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pots, CONFIG_POTS_LOG_LEVEL);

struct pots_config {
    struct gpio_dt_spec mux;
    const struct adc_dt_spec *adc_specs;
};

const struct device *ext_power_dev = DEVICE_DT_GET(DT_NODELABEL(ext_power));

struct adc_sequence seq = {
    .buffer_size = 3 * sizeof(uint16_t),
    .resolution = 10,
    .oversampling = NRF_SAADC_OVERSAMPLE_DISABLED,
};

static int pots_read(const struct device *dev, uint16_t *sample_buf) {
    const struct pots_config *config = dev->config;
    int ret;

    ext_power_set_state(ext_power_dev, 1);

    for (int xy = 0; xy <= 1; xy++) {
        ret = gpio_pin_set_dt(&config->mux, xy);
        if (ret < 0) return ret;
        k_busy_wait(5);  // allow mux to settle

        seq.buffer = &sample_buf[xy * 3];

        // assume all channels are on the same ADC
        ret = adc_read_dt(&config->adc_specs[0], &seq);
        if (ret < 0) return ret;
    }

    // mitigation for a probable nrfx_saadc bug
    // resulting in periperal/CPU not going to sleep after multi channel read
    nrfx_saadc_abort();

    ext_power_set_state(ext_power_dev, 0);
    return 0;
}

static DEVICE_API(pots, pots_api) = {
    .pots_read = &pots_read,
};

static int pots_init(const struct device *dev) {
    const struct pots_config *config = dev->config;

    if (!gpio_is_ready_dt(&config->mux)) {
        LOG_ERR("Mux GPIO not ready");
        return -ENODEV;
    }
    int ret = gpio_pin_configure_dt(&config->mux, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) return ret;

    for (int i = 0; i < 3; i++) {
        if (!adc_is_ready_dt(&config->adc_specs[i])) return -ENODEV;
        struct adc_channel_cfg channel_cfg = {
            .channel_id = config->adc_specs[i].channel_id,
            .gain = ADC_GAIN_1_6,
            .reference = ADC_REF_INTERNAL,
            .acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
            .input_positive = config->adc_specs[i].channel_id + 1,
        };
        ret = adc_channel_setup(config->adc_specs[i].dev, &channel_cfg);
        if (ret < 0) return -ENODEV;
    }

    for (int i = 0; i < 3; i++) {
        uint8_t ch = config->adc_specs[i].channel_id;
        seq.channels |= BIT(ch);
    }

    return 0;
}

#define DT_SPEC_AND_COMMA(node_id, prop, idx) ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

#define POTS_DRIVER_DEFINE(inst)                                \
    static struct pots_data pots_data_##inst;                   \
    static const struct adc_dt_spec pots_adc_specs_##inst[] = { \
        DT_FOREACH_PROP_ELEM(DT_DRV_INST(inst), io_channels,    \
                             DT_SPEC_AND_COMMA)};               \
    static const struct pots_config pots_config_##inst = {      \
        .mux = GPIO_DT_SPEC_INST_GET(inst, mux_gpios),          \
        .adc_specs = pots_adc_specs_##inst};                    \
                                                                \
    DEVICE_DT_INST_DEFINE(inst,                                 \
                          pots_init,                            \
                          NULL,                                 \
                          &pots_data_##inst,                    \
                          &pots_config_##inst,                  \
                          POST_KERNEL,                          \
                          CONFIG_POTS_INIT_PRIORITY,            \
                          &pots_api);

DT_INST_FOREACH_STATUS_OKAY(POTS_DRIVER_DEFINE)