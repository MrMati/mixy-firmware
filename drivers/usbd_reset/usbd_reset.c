#define DT_DRV_COMPAT mixy_reset

#include <zephyr/drivers/usb/udc.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/usb/class/usb_cdc.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>

LOG_MODULE_REGISTER(usbd_reset, CONFIG_USBD_LOG_LEVEL);

#define CDC_ACM_DEFAULT_INT_EP_MPS 16
#define CDC_ACM_FS_INT_EP_INTERVAL USB_FS_INT_EP_INTERVAL(10000U)

struct usbd_cdc_acm_desc {
    struct usb_association_descriptor iad;
    struct usb_if_descriptor if0;
    struct cdc_header_descriptor if0_header;
    struct cdc_cm_descriptor if0_cm;
    struct cdc_acm_descriptor if0_acm;
    struct cdc_union_descriptor if0_union;
    struct usb_ep_descriptor if0_int_ep;

    struct usb_if_descriptor if1;
    struct usb_ep_descriptor if1_in_ep;
    struct usb_ep_descriptor if1_out_ep;

    struct usb_desc_header nil_desc;
};

struct cdc_acm_config {
    /* Pointer to the associated USBD class node */
    struct usbd_class_data *c_data;
    /* Pointer to the interface description node or NULL */
    struct usbd_desc_node *const if_desc_data;
    /* Pointer to the class interface descriptors */
    struct usbd_cdc_acm_desc *const desc;
    const struct usb_desc_header **const fs_desc;
};

struct cdc_acm_data {
    const struct device *dev;

    struct cdc_acm_line_coding line_coding;
};

static void *usbd_cdc_acm_get_desc(struct usbd_class_data *const c_data,
                                   const enum usbd_speed speed) {
    const struct device *dev = usbd_class_get_private(c_data);
    const struct cdc_acm_config *cfg = dev->config;

    return cfg->fs_desc;
}

static int usbd_cdc_acm_request(struct usbd_class_data *const c_data,
                                struct net_buf *buf, int err) {
    struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
    struct udc_buf_info *bi;

    bi = udc_get_buf_info(buf);
    return usbd_ep_buf_free(uds_ctx, buf);
}

static int usbd_reset_ctd(struct usbd_class_data *const c_data,
                          const struct usb_setup_packet *const setup,
                          const struct net_buf *const buf) {
    struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
    const struct device *dev = usbd_class_get_private(c_data);
    struct cdc_acm_data *data = dev->data;

    if (setup->bRequest == SET_LINE_CODING) {
        size_t len = sizeof(data->line_coding);
        if (setup->wLength != len) {
            errno = -ENOTSUP;
            return 0;
        }

        memcpy(&data->line_coding, buf->data, len);

        if (data->line_coding.dwDTERate == 1200) {
            LOG_WRN("Received reset command, rebooting into bootloader...");
            log_flush();

            udc_shutdown(uds_ctx->dev);

            NRF_POWER->GPREGRET = 0x57;
            NVIC_SystemReset();
        }
    }

    LOG_DBG("bmRequestType 0x%02x bRequest 0x%02x",
            setup->bmRequestType, setup->bRequest);
    errno = -ENOTSUP;
    return 0;
}

static int usbd_cdc_acm_init(struct usbd_class_data *const c_data) {
    struct usbd_context *uds_ctx = usbd_class_get_ctx(c_data);
    const struct device *dev = usbd_class_get_private(c_data);
    const struct cdc_acm_config *cfg = dev->config;
    struct usbd_cdc_acm_desc *desc = cfg->desc;

    desc->if0_union.bControlInterface = desc->if0.bInterfaceNumber;
    desc->if0_union.bSubordinateInterface0 = desc->if1.bInterfaceNumber;

    if (cfg->if_desc_data != NULL && desc->if0.iInterface == 0) {
        if (usbd_add_descriptor(uds_ctx, cfg->if_desc_data)) {
            LOG_ERR("Failed to add interface string descriptor");
        } else {
            desc->if0.iInterface = usbd_str_desc_get_idx(cfg->if_desc_data);
        }
    }

    return 0;
}

static int usbd_cdc_acm_cth(struct usbd_class_data *const c_data,
                            const struct usb_setup_packet *const setup,
                            struct net_buf *const buf) {
    const struct device *dev = usbd_class_get_private(c_data);
    struct cdc_acm_data *data = dev->data;
    size_t min_len;

    if (setup->bRequest == GET_LINE_CODING) {
        if (buf == NULL) {
            errno = -ENOMEM;
            return 0;
        }

        // Windows usbser.sys is happier with this
        min_len = MIN(sizeof(data->line_coding), setup->wLength);
        net_buf_add_mem(buf, &data->line_coding, min_len);

        return 0;
    }

    LOG_DBG("bmRequestType 0x%02x bRequest 0x%02x unsupported",
            setup->bmRequestType, setup->bRequest);
    errno = -ENOTSUP;

    return 0;
}

struct usbd_class_api usbd_cdc_acm_api = {
    .request = usbd_cdc_acm_request,
    .control_to_dev = usbd_reset_ctd,
    .control_to_host = usbd_cdc_acm_cth,
    .get_desc = usbd_cdc_acm_get_desc,
    .init = usbd_cdc_acm_init,
};

#define CDC_ACM_DEFINE_DESCRIPTOR(n)                                                       \
    static struct usbd_cdc_acm_desc cdc_acm_desc_##n = {                                   \
        .iad = {                                                                           \
            .bLength = sizeof(struct usb_association_descriptor),                          \
            .bDescriptorType = USB_DESC_INTERFACE_ASSOC,                                   \
            .bFirstInterface = 0,                                                          \
            .bInterfaceCount = 0x02,                                                       \
            .bFunctionClass = USB_BCC_CDC_CONTROL,                                         \
            .bFunctionSubClass = ACM_SUBCLASS,                                             \
            .bFunctionProtocol = 0,                                                        \
            .iFunction = 0,                                                                \
        },                                                                                 \
                                                                                           \
        .if0 = {                                                                           \
            .bLength = sizeof(struct usb_if_descriptor),                                   \
            .bDescriptorType = USB_DESC_INTERFACE,                                         \
            .bInterfaceNumber = 0,                                                         \
            .bAlternateSetting = 0,                                                        \
            .bNumEndpoints = 1,                                                            \
            .bInterfaceClass = USB_BCC_CDC_CONTROL,                                        \
            .bInterfaceSubClass = ACM_SUBCLASS,                                            \
            .bInterfaceProtocol = 0,                                                       \
            .iInterface = 0,                                                               \
        },                                                                                 \
                                                                                           \
        .if0_header = {                                                                    \
            .bFunctionLength = sizeof(struct cdc_header_descriptor),                       \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                      \
            .bDescriptorSubtype = HEADER_FUNC_DESC,                                        \
            .bcdCDC = sys_cpu_to_le16(USB_SRN_1_1),                                        \
        },                                                                                 \
                                                                                           \
        .if0_cm = {                                                                        \
            .bFunctionLength = sizeof(struct cdc_cm_descriptor),                           \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                      \
            .bDescriptorSubtype = CALL_MANAGEMENT_FUNC_DESC,                               \
            .bmCapabilities = 0,                                                           \
            .bDataInterface = 1,                                                           \
        },                                                                                 \
                                                                                           \
        .if0_acm = {                                                                       \
            .bFunctionLength = sizeof(struct cdc_acm_descriptor),                          \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                      \
            .bDescriptorSubtype = ACM_FUNC_DESC, /* See CDC PSTN Subclass Chapter 5.3.2 */ \
            .bmCapabilities = BIT(1),                                                      \
        },                                                                                 \
                                                                                           \
        .if0_union = {                                                                     \
            .bFunctionLength = sizeof(struct cdc_union_descriptor),                        \
            .bDescriptorType = USB_DESC_CS_INTERFACE,                                      \
            .bDescriptorSubtype = UNION_FUNC_DESC,                                         \
            .bControlInterface = 0,                                                        \
            .bSubordinateInterface0 = 1,                                                   \
        },                                                                                 \
                                                                                           \
        .if0_int_ep = {                                                                    \
            .bLength = sizeof(struct usb_ep_descriptor),                                   \
            .bDescriptorType = USB_DESC_ENDPOINT,                                          \
            .bEndpointAddress = 0x81,                                                      \
            .bmAttributes = USB_EP_TYPE_INTERRUPT,                                         \
            .wMaxPacketSize = sys_cpu_to_le16(CDC_ACM_DEFAULT_INT_EP_MPS),                 \
            .bInterval = CDC_ACM_FS_INT_EP_INTERVAL,                                       \
        },                                                                                 \
                                                                                           \
        .if1 = {                                                                           \
            .bLength = sizeof(struct usb_if_descriptor),                                   \
            .bDescriptorType = USB_DESC_INTERFACE,                                         \
            .bInterfaceNumber = 1,                                                         \
            .bAlternateSetting = 0,                                                        \
            .bNumEndpoints = 2,                                                            \
            .bInterfaceClass = USB_BCC_CDC_DATA,                                           \
            .bInterfaceSubClass = 0,                                                       \
            .bInterfaceProtocol = 0,                                                       \
            .iInterface = 0,                                                               \
        },                                                                                 \
                                                                                           \
        .if1_in_ep = {                                                                     \
            .bLength = sizeof(struct usb_ep_descriptor),                                   \
            .bDescriptorType = USB_DESC_ENDPOINT,                                          \
            .bEndpointAddress = 0x82,                                                      \
            .bmAttributes = USB_EP_TYPE_BULK,                                              \
            .wMaxPacketSize = sys_cpu_to_le16(64U),                                        \
            .bInterval = 0,                                                                \
        },                                                                                 \
                                                                                           \
        .if1_out_ep = {                                                                    \
            .bLength = sizeof(struct usb_ep_descriptor),                                   \
            .bDescriptorType = USB_DESC_ENDPOINT,                                          \
            .bEndpointAddress = 0x01,                                                      \
            .bmAttributes = USB_EP_TYPE_BULK,                                              \
            .wMaxPacketSize = sys_cpu_to_le16(64U),                                        \
            .bInterval = 0,                                                                \
        },                                                                                 \
                                                                                           \
        .nil_desc = {                                                                      \
            .bLength = 0,                                                                  \
            .bDescriptorType = 0,                                                          \
        },                                                                                 \
    };                                                                                     \
                                                                                           \
    const static struct usb_desc_header *cdc_acm_fs_desc_##n[] = {                         \
        (struct usb_desc_header *)&cdc_acm_desc_##n.iad,                                   \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if0,                                   \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if0_header,                            \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if0_cm,                                \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if0_acm,                               \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if0_union,                             \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if0_int_ep,                            \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if1,                                   \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if1_in_ep,                             \
        (struct usb_desc_header *)&cdc_acm_desc_##n.if1_out_ep,                            \
        (struct usb_desc_header *)&cdc_acm_desc_##n.nil_desc,                              \
    }

#define USBD_RESET_DT_DEVICE_DEFINE(n)                                                                    \
    BUILD_ASSERT(DT_INST_ON_BUS(n, usb),                                                                  \
                 "node " DT_NODE_PATH(DT_DRV_INST(n)) " is not assigned to a USB device controller");     \
                                                                                                          \
    CDC_ACM_DEFINE_DESCRIPTOR(n);                                                                         \
                                                                                                          \
    USBD_DEFINE_CLASS(reset_##n,                                                                          \
                      &usbd_cdc_acm_api,                                                                  \
                      (void *)DEVICE_DT_GET(DT_DRV_INST(n)), NULL);                                       \
                                                                                                          \
    IF_ENABLED(DT_INST_NODE_HAS_PROP(n, label), (                                                         \
                                                    USBD_DESC_STRING_DEFINE(cdc_acm_if_desc_data_##n,     \
                                                                            DT_INST_PROP(n, label),       \
                                                                            USBD_DUT_STRING_INTERFACE);)) \
                                                                                                          \
    static const struct cdc_acm_config reset_config_##n = {                                               \
        .c_data = &reset_##n,                                                                             \
        IF_ENABLED(DT_INST_NODE_HAS_PROP(n, label), (                                                     \
                                                            .if_desc_data = &cdc_acm_if_desc_data_##n, )) \
            .desc = &cdc_acm_desc_##n,                                                                    \
        .fs_desc = cdc_acm_fs_desc_##n,                                                                   \
    };                                                                                                    \
                                                                                                          \
    static struct cdc_acm_data reset_data_##n = {                                                         \
        .dev = DEVICE_DT_GET(DT_DRV_INST(n)),                                                             \
    };                                                                                                    \
                                                                                                          \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL,                                                                  \
                          &reset_data_##n, &reset_config_##n,                                             \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,                                \
                          NULL);

DT_INST_FOREACH_STATUS_OKAY(USBD_RESET_DT_DEVICE_DEFINE);
