zephyr_include_directories(.)

zephyr_library()

zephyr_library_sources(battery_common.c)
zephyr_library_sources_ifdef(CONFIG_MIXY_BATTERY_NRF_VDDH battery_nrf_vddh.c)