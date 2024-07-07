# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/thetragedyofdarthwise/pico/pico-sdk/tools/pioasm"
  "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pioasm"
  "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pico-sdk/src/rp2_common/tinyusb/pioasm"
  "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pico-sdk/src/rp2_common/tinyusb/pioasm/tmp"
  "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pico-sdk/src/rp2_common/tinyusb/pioasm/src/PioasmBuild-stamp"
  "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pico-sdk/src/rp2_common/tinyusb/pioasm/src"
  "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pico-sdk/src/rp2_common/tinyusb/pioasm/src/PioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pico-sdk/src/rp2_common/tinyusb/pioasm/src/PioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/thetragedyofdarthwise/PicoDev/explorer/MUX/src/test/pico-sdk/src/rp2_common/tinyusb/pioasm/src/PioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
