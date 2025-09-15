#ifndef APP_DRIVERS_POTS_H_
#define APP_DRIVERS_POTS_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

struct pots_data {
};

__subsystem struct pots_driver_api {
	int (*pots_read)(const struct device *dev, uint16_t *sample_buf);
};


__syscall int mixy_pots_read(const struct device *dev, uint16_t *sample_buf);

static inline int z_impl_mixy_pots_read(const struct device *dev, uint16_t *sample_buf)
{
	__ASSERT_NO_MSG(DEVICE_API_IS(pots, dev));

	return DEVICE_API_GET(pots, dev)->pots_read(dev, sample_buf);
}

#include <syscalls/pots.h>

#endif /* APP_DRIVERS_POTS_H_ */
