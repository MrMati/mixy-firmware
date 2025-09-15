#include <nrfx_power.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usbd.h>

LOG_MODULE_REGISTER(usbd_reset_register, CONFIG_USBD_LOG_LEVEL);

USBD_DEVICE_DEFINE(reset_interface,
                   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
                   CONFIG_RESET_INTERFACE_VID, CONFIG_RESET_INTERFACE_PID);

USBD_DESC_LANG_DEFINE(reset_interface_lang);
USBD_DESC_MANUFACTURER_DEFINE(reset_interface_mfr, CONFIG_RESET_INTERFACE_MANUFACTURER_STRING);
USBD_DESC_PRODUCT_DEFINE(reset_interface_product, CONFIG_RESET_INTERFACE_PRODUCT_STRING);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(reset_interface_sn)));

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");

static const uint8_t attributes = IS_ENABLED(CONFIG_RESET_INTERFACE_SELF_POWERED) ? USB_SCD_SELF_POWERED : 0;

USBD_CONFIGURATION_DEFINE(reset_interface_fs_config,
                          attributes,
                          CONFIG_RESET_INTERFACE_MAX_POWER, &fs_cfg_desc);

static void disable_usb(struct k_work *work) {
    if (usbd_disable(&reset_interface)) {
        LOG_ERR("Failed to disable usbd");
    }
}

static K_WORK_DELAYABLE_DEFINE(disable_usb_work, disable_usb);

static void msg_cb(struct usbd_context *const usbd_ctx,
                   const struct usbd_msg *const msg) {
    if (usbd_can_detect_vbus(usbd_ctx)) {
        if (msg->type == USBD_MSG_VBUS_READY) {
            if (usbd_enable(usbd_ctx)) {
                LOG_ERR("Failed to enable usbd");
            } else {
                k_work_cancel_delayable(&disable_usb_work);
                k_work_schedule(&disable_usb_work, K_MSEC(2000));
            }
        }

        if (msg->type == USBD_MSG_VBUS_REMOVED) {
            if (usbd_disable(usbd_ctx)) {
                LOG_ERR("Failed to disable usbd");
            }
        }
    }
}

static int register_reset_0(struct usbd_context *const uds_ctx,
                            const enum usbd_speed speed) {
    struct usbd_config_node *cfg_nd;
    int err;

    cfg_nd = &reset_interface_fs_config;

    err = usbd_add_configuration(uds_ctx, speed, cfg_nd);
    if (err) {
        LOG_ERR("Failed to add configuration");
        return err;
    }

    err = usbd_register_class(&reset_interface, "reset_0", speed, 1);
    if (err) {
        LOG_ERR("Failed to register classes");
        return err;
    }

    return usbd_device_set_code_triple(uds_ctx, speed,
                                       USB_BCC_MISCELLANEOUS, 0x02, 0x01);
}

static int reset_interface_init_device(void) {
    int err;

    err = usbd_add_descriptor(&reset_interface, &reset_interface_lang);
    if (err) {
        LOG_ERR("Failed to initialize %s (%d)", "language descriptor", err);
        return err;
    }

    err = usbd_add_descriptor(&reset_interface, &reset_interface_mfr);
    if (err) {
        LOG_ERR("Failed to initialize %s (%d)", "manufacturer descriptor", err);
        return err;
    }

    err = usbd_add_descriptor(&reset_interface, &reset_interface_product);
    if (err) {
        LOG_ERR("Failed to initialize %s (%d)", "product descriptor", err);
        return err;
    }

    if (IS_ENABLED(CONFIG_HWINFO)) {
        err = usbd_add_descriptor(&reset_interface, &reset_interface_sn);
        if (err) {
            LOG_ERR("Failed to initialize %s (%d)", "SN descriptor", err);
            return err;
        }
    }

    err = register_reset_0(&reset_interface, USBD_SPEED_FS);
    if (err) {
        return err;
    }

    err = usbd_init(&reset_interface);
    if (err) {
        LOG_ERR("Failed to initialize reset interface (%d)", err);
        return err;
    }

    err = usbd_msg_register_cb(&reset_interface, msg_cb);
    if (err) {
        LOG_ERR("Failed to register usbd message callback (%d)", err);
        return err;
    }

    if (IS_ENABLED(CONFIG_RESET_INTERFACE_ENABLE_AT_BOOT) || nrfx_power_usbstatus_get() != NRFX_POWER_USB_STATE_DISCONNECTED) {
        err = usbd_enable(&reset_interface);
        if (err) {
            LOG_ERR("Failed to enable reset interface (%d)", err);
            return err;
        }
        k_work_schedule(&disable_usb_work, K_MSEC(2000));
    }



    return 0;
}

SYS_INIT(reset_interface_init_device, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
