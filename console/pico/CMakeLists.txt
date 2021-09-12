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
if ( ${STDIO} STREQUAL "PICO" )
  include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
endif()

project(bbcbasic C CXX ASM)
pico_sdk_init()

if (TARGET tinyusb_device)
  add_executable(bbcbasic 
    ../../src/bbmain.c
    ../../src/bbexec.c
    ../../src/bbeval.c
    ../../src/bbasmb_arm_32.c 
    ../../src/bbccos.c
    ../../src/bbccon.c
    ../../src/bbdata_arm_32.s
    ../../mcu/sympico.c
    ../../mcu/lfswrap.c 
    ../../mcu/m0FaultDispatch/m0FaultDispatch.c
    ../../mcu/fault.c
    )
  pico_set_printf_implementation(bbcbasic compiler)
  target_compile_options(bbcbasic PRIVATE -O1 -g)
  target_link_options(bbcbasic PRIVATE -g)
  target_link_libraries(bbcbasic
    pico_stdlib
    hardware_flash
    hardware_exception
    )
  target_compile_definitions(bbcbasic PUBLIC PICO)
  target_include_directories(bbcbasic PRIVATE
    ../../include
    ../../mcu
    ../../mcu/m0FaultDispatch
    )

  if ( ${LFS} )
    message(STATUS "Including LFS filesystem for flash storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_LFS)
    target_sources(bbcbasic PRIVATE
      ../../mcu/lfspico.c
      ../../mcu/littlefs/lfs.c
      ../../mcu/littlefs/lfs_util.c
      )
    target_include_directories(bbcbasic PRIVATE
      ../../mcu/littlefs
      )
  endif()

  if ( ${FAT} )
    message(STATUS "Including FAT filesystem for SD card storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_FAT)
    pico_generate_pio_header(bbcbasic ${CMAKE_CURRENT_LIST_DIR}/../../mcu/sd_spi.pio)
    target_sources(bbcbasic PRIVATE
      ../../mcu/fatfs/ff.c
      ../../mcu/fatfs/ffsystem.c
      ../../mcu/fatfs/ffunicode.c
      ../../mcu/ff_disk.c
      ../../mcu/sd_spi2.c
      )
    target_link_libraries(bbcbasic hardware_dma hardware_pio)
  endif()

  if ( NOT STDIO )
    set(STDIO "USB")
  endif()
  if ( ${STDIO} STREQUAL "PICO" )
    message(STATUS "BBC Basic console I/O will be on USB keyboard and VGA screen")
    pico_enable_stdio_usb(bbcbasic 0)
    pico_enable_stdio_uart(bbcbasic 1)
    target_sources(bbcbasic PRIVATE
      ../../mcu/picokbd.c
      ../../mcu/picovdu.c
      ../../mcu/framebuffer.S
      )
    target_compile_definitions(bbcbasic PUBLIC
      PICO_STACK_SIZE=0x2B800
      PICO_CORE1_STACK_SIZE=0x800
      )
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_PICO)
    # target_compile_definitions(bbcbasic PRIVATE -DSTDIO_UART)
    target_compile_definitions(bbcbasic PRIVATE -DPICO_VGA)
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=402
      -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
      )
    target_link_libraries(bbcbasic
      pico_scanvideo_dpi
      pico_multicore
      hardware_interp
      tinyusb_host
      tinyusb_board
      )
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/vga_pico.ld)
  elseif ( ${STDIO} STREQUAL "UART" )
    message(STATUS "BBC Basic console I/O will be on UART")
    pico_enable_stdio_usb(bbcbasic 0)
    pico_enable_stdio_uart(bbcbasic 1)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_UART)
    target_compile_definitions(bbcbasic PUBLIC PICO PICO_STACK_SIZE=0x3C000)
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico.ld)
  else()
    message(STATUS "BBC Basic console I/O will be on USB")
    pico_enable_stdio_usb(bbcbasic 1)
    pico_enable_stdio_uart(bbcbasic 0)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB)
    target_compile_definitions(bbcbasic PUBLIC PICO PICO_STACK_SIZE=0x3C000)
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico.ld)
  endif()
  
  pico_add_extra_outputs(bbcbasic)
elseif(PICO_ON_DEVICE)
  message(WARNING "Not building bbcbasic because TinyUSB submodule is not initialized in the SDK")
endif()
