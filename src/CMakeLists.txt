cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(SDRead)

set(CMAKE_C_STANDARD 11)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
#include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)


add_executable(SDRead
	main.c 
	../ssd1306.c
	bootrom_api.c
)

target_include_directories(SDRead
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../
)




target_link_libraries(SDRead
	pico_stdlib 
	hardware_spi
	hardware_pll
	hardware_clocks
	hardware_gpio
	hardware_adc
	pico_multicore
	pico_bootrom
	hardware_i2c)

pico_enable_stdio_usb(SDRead 1) 
pico_enable_stdio_uart(SDRead 0) 

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(SDRead)

