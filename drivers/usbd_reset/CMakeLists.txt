zephyr_include_directories(.)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_USBD_RESET_CLASS usbd_reset.c)