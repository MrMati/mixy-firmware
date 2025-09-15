# Mixy firmware


This repository contains firmware for the beta version of Mixy, a BLE audio mixer.
You can learn more about it in main [repo](https://github.com/MrMati/mixy).

## How to build

Firstly, install west as described in Zephyr's [Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

Use this repo as manifest for the workspace:

```shell
west init -m https://github.com/MrMati/mixy-firmware my-workspace
cd my-workspace
west update
```

To build the application, run the following command:

```shell
west build -b nice_nano_v2 app
```

To enable bootloader reset interface, use additional USB config:

```shell
west build -b nice_nano_v2 app -- -DEXTRA_CONF_FILE=log.conf
```

Once you have built the firmware, run the following command to flash it (TODO):

```shell
west flash
```

