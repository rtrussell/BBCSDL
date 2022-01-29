#  This file is for using the SDK to create software
#  that runs on the Raspberry Pi Pico.  Please set
#
#     PICO_SDK_PATH
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

if ( NOT STDIO )
  set(STDIO "USB+UART")
endif()

if (TARGET tinyusb_device)
  add_executable(bbcbasic 
    ../../BBCSDL/src/bbdata_arm_32.s
    ../../BBCSDL/src/bbmain.c
    ../../src/bbccos.c
    ../../src/bbpico.c
    ../../src/bbasmb_arm_v6m.c 
    ../../src/lfswrap.c 
    ../../src/fault.c
    ../../src/pico/sympico.c
    )
  if (${MIN_STACK})
    message(STATUS "Expression evaluation revised to reduce stack usage")
    target_sources(bbcbasic PRIVATE
      ../../src/bbexec2.c
      ../../src/bbeval2.c
      )
  else()
    message(STATUS "Using upstream expression evaluation code")
    target_sources(bbcbasic PRIVATE
      ../../BBCSDL/src/bbexec.c
      ../../BBCSDL/src/bbeval.c
      )
  endif()
  if (${STACK_CHECK} GREATER_EQUAL 4)
    message(STATUS "Using memory protection to detect stack overrun")
    target_sources(bbcbasic PRIVATE ../../src/pico/stack_trap.c)
  else()
    target_sources(bbcbasic PRIVATE ../../m0FaultDispatch/m0FaultDispatch.c)
  endif()
      
  pico_set_printf_implementation(bbcbasic compiler)
  target_compile_options(bbcbasic PRIVATE -O1 -g)
  target_link_options(bbcbasic PRIVATE -g)
  target_link_libraries(bbcbasic
    pico_stdlib
    hardware_flash
    hardware_exception
    pico_printf_headers
    )
  target_compile_definitions(bbcbasic PUBLIC PICO)
  target_compile_definitions(bbcbasic PUBLIC PICO_STACK_CHECK=${STACK_CHECK})
  target_include_directories(bbcbasic PRIVATE
    ../../include
    ../../BBCSDL/include
    ../../m0FaultDispatch
    )

  if ( ${SOUND} STREQUAL "I2S")
    message(STATUS "Generate sound using I2S output")
    target_compile_definitions(bbcbasic PUBLIC -DPICO_SOUND=1)
    pico_generate_pio_header(bbcbasic ${CMAKE_CURRENT_LIST_DIR}/../../src/pico/sound.pio)
    target_sources(bbcbasic PRIVATE
      ../../src/sn76489.c
      ../../src/pico/pico_snd.c
      )
  elseif ( ${SOUND} STREQUAL "PWM")
    message(STATUS "Generate sound using PWM")
    target_compile_definitions(bbcbasic PUBLIC -DPICO_SOUND=2)
    target_sources(bbcbasic PRIVATE
      ../../src/sn76489.c
      ../../src/pico/pico_snd.c
      )
    target_link_libraries(bbcbasic hardware_pwm)
  endif()

  if ( ${LFS} )
    message(STATUS "Including LFS filesystem for flash storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_LFS)
    target_sources(bbcbasic PRIVATE
      ../../src/lfsmcu.c
      ../../littlefs/lfs.c
      ../../littlefs/lfs_util.c
      )
    target_include_directories(bbcbasic PRIVATE
      ../../littlefs
      )
  endif()

  if ( ${FAT} )
    message(STATUS "Including FAT filesystem for SD card storage")
    target_compile_definitions(bbcbasic PUBLIC -DHAVE_FAT)
    pico_generate_pio_header(bbcbasic ${CMAKE_CURRENT_LIST_DIR}/../../src/pico/sd_spi.pio)
    target_sources(bbcbasic PRIVATE
      ../../fatfs/ff.c
      ../../fatfs/ffsystem.c
      ../../fatfs/ffunicode.c
      ../../src/pico/ff_disk.c
      ../../src/pico/sd_spi2.c
      )
    target_include_directories(bbcbasic PRIVATE
      ../../include/pico
      )
    target_link_libraries(bbcbasic hardware_dma hardware_pio)
  endif()

  if ( ${LFS} OR ${FAT} )
    target_compile_definitions(bbcbasic PRIVATE -DHAVE_DEV)
    if ( NOT ( ${STDIO} STREQUAL "PICO" ))
      target_sources(bbcbasic PRIVATE ../../src/pico/picoser.c)
    endif()
  endif()
  
  if ( ${STDIO} STREQUAL "PICO" )
    message(STATUS "BBC Basic console I/O will be on USB keyboard and VGA screen")
    pico_enable_stdio_usb(bbcbasic 0)
    pico_enable_stdio_uart(bbcbasic 1)
    target_sources(bbcbasic PRIVATE
      ../../src/fbufvdu.c
      ../../src/pico_gui/picokbd.c
      ../../src/pico_gui/picofbuf.c
      ../../src/pico_gui/framebuffer.S
      )
    target_compile_definitions(bbcbasic PUBLIC
      PICO_STACK_SIZE=0x2B800
      PICO_CORE1_STACK_SIZE=0x800
      )
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_PICO)
    target_compile_definitions(bbcbasic PRIVATE -DPICO_GUI)
    
    target_compile_definitions(bbcbasic PUBLIC -DPICO_PANIC_FUNCTION=message)
    target_compile_definitions(bbcbasic PUBLIC
      -DPICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=402
      -DPICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
      )
    target_include_directories(bbcbasic PRIVATE
      ../../include/pico
      ../../include/pico_gui
      )
    target_link_libraries(bbcbasic
      pico_scanvideo_dpi
      pico_multicore
      hardware_interp
      tinyusb_host
      tinyusb_board
      )
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/../pico_gui/gui_pico.ld)
  elseif ( ${STDIO} STREQUAL "USB+UART" )
    message(STATUS "BBC Basic console I/O will be on USB or UART")
    pico_enable_stdio_usb(bbcbasic 1)
    pico_enable_stdio_uart(bbcbasic 1)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB -DSTDIO_UART)
    target_compile_definitions(bbcbasic PUBLIC PICO_STACK_SIZE=0x3C000)
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico.ld)
  elseif ( ${STDIO} STREQUAL "UART" )
    message(STATUS "BBC Basic console I/O will be on UART")
    pico_enable_stdio_usb(bbcbasic 0)
    pico_enable_stdio_uart(bbcbasic 1)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_UART)
    target_compile_definitions(bbcbasic PUBLIC PICO_STACK_SIZE=0x3C000)
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico.ld)
  else()
    message(STATUS "BBC Basic console I/O will be on USB")
    pico_enable_stdio_usb(bbcbasic 1)
    pico_enable_stdio_uart(bbcbasic 0)
    target_compile_definitions(bbcbasic PRIVATE -DSTDIO_USB)
    target_compile_definitions(bbcbasic PUBLIC PICO_STACK_SIZE=0x3C000)
    pico_set_linker_script(bbcbasic ${CMAKE_SOURCE_DIR}/bbc_pico.ld)
  endif()
  
  pico_add_extra_outputs(bbcbasic)
elseif(PICO_ON_DEVICE)
  message(WARNING "Not building bbcbasic because TinyUSB submodule is not initialized in the SDK")
endif()
