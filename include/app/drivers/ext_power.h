#ifndef APP_DRIVERS_EXT_POWER_H_
#define APP_DRIVERS_EXT_POWER_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

__subsystem struct ext_power_driver_api {
    int (*set_state)(const struct device *dev, int state);
};

__syscall int ext_power_set_state(const struct device *dev,
                                  int state);

static inline int z_impl_ext_power_set_state(const struct device *dev, int state) {
    __ASSERT_NO_MSG(DEVICE_API_IS(ext_power, dev));

    return DEVICE_API_GET(ext_power, dev)->set_state(dev, state);
}

#include <syscalls/ext_power.h>

/** @} */

/** @} */

#endif /* APP_DRIVERS_EXT_POWER_H_ */
