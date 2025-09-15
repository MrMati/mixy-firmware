cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE
  ${app_sources}
  )

target_sources_ifdef(CONFIG_RESET_INTERFACE_INITIALIZE_AT_BOOT 
  app PRIVATE  
  src/utils/usbd_reset_register.c
)