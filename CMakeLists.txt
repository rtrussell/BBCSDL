#  If building on Linux or other Posix-like operating system simply
#  use the Makefile.  This file is for using the SDK to create a 
#  that runs on the Raspberry Pi Pico.  Please update
#
#     PICO_SKD_PATH
#
#  to the subdirectory where you have installed the SDK. */

cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(bbcbasic C CXX ASM)
pico_sdk_init()

if (TARGET tinyusb_device)
  add_executable(bbcbasic 
    src/bbmain.c src/bbexec.c src/bbeval.c src/bbasmb_arm_32.c 
    src/bbccos.c src/bbccon.c src/lfswrap.c 
    m0FaultDispatch/m0FaultDispatch.c src/fault.c
    src/bbdata_arm_32.s)
  target_compile_options(bbcbasic PRIVATE -O1 -g)
  target_link_options(bbcbasic PRIVATE -g)
  target_link_libraries(bbcbasic pico_stdlib hardware_flash hardware_exception)
  target_compile_definitions(bbcbasic PUBLIC PICO PICO_STACK_SIZE=0x3C000)
  target_include_directories(bbcbasic PRIVATE include m0FaultDispatch)
  pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico.ld)

  if ( ${LFS} )
    message(STATUS "Including LFS filesystem for flash storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_LFS)
    target_sources(bbcbasic PRIVATE src/lfspico.c littlefs/lfs.c littlefs/lfs_util.c)
    target_include_directories(bbcbasic PRIVATE littlefs)
  endif()

  if ( ${FAT} )
    message(STATUS "Including FAT filesystem for SD card storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_FAT)
    pico_generate_pio_header(bbcbasic ${CMAKE_CURRENT_LIST_DIR}/src/sd_spi.pio)
    target_sources(bbcbasic PRIVATE fatfs/ff.c fatfs/ffsystem.c fatfs/ffunicode.c
      src/ff_disk.c src/sd_spi2.c)
    target_link_libraries(bbcbasic hardware_dma hardware_pio)
  endif()

  if ( NOT STDIO )
    set(STDIO "USB")
  endif()
  if ( ${STDIO} STREQUAL "UART" )
    message(STATUS "BBC Basic console I/O will be on UART")
    pico_enable_stdio_usb(bbcbasic 0)
    pico_enable_stdio_uart(bbcbasic 1)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_UART)
  else()
    message(STATUS "BBC Basic console I/O will be on USB")
    pico_enable_stdio_usb(bbcbasic 1)
    pico_enable_stdio_uart(bbcbasic 0)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB)
  endif()
  
  pico_add_extra_outputs(bbcbasic)
elseif(PICO_ON_DEVICE)
  message(WARNING "Not building bbcbasic because TinyUSB submodule is not initialized in the SDK")
endif()
