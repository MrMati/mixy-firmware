zephyr_library()
zephyr_library_sources_ifdef(CONFIG_EXT_POWER ext_power.c)
