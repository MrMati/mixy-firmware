# Out-of-tree drivers for custom classes
add_subdirectory_ifdef(CONFIG_EXT_POWER ext_power)
add_subdirectory_ifdef(CONFIG_POTS pots)
add_subdirectory_ifdef(CONFIG_USBD_RESET_CLASS usbd_reset)

# Out-of-tree drivers for sensor drivers
add_subdirectory_ifdef(CONFIG_SENSOR sensor)


