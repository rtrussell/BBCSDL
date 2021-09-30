# BBCSDL

For the original BBCSDL please go to https://github.com/rtrussell/BBCSDL

This fork is part of an attempt to implement BBC Basic on a Raspberry Pi Pico.
For discussion see https://www.raspberrypi.org/forums/viewtopic.php?f=144&t=316761

It includes work by:

* R. T. Russell
* Eric Olson
* Myself

Apologies to anyone else I omitted.

There are two somewhat divergent lines of development:

1.  To be able to use the Pico as a microcontroller programmed in BBC Basic.
    User interaction (if any) would be via a console interface over USB or serial.
    Includes program and data storage either in Pico flash and/or an attached SD card.
    This is in a fairly advanced state of development.

2.  To be able to use the Pico as a computer programmable in BBC Basic with input
    by an attached USB keyboard and display on an attached VGA monitor. This
    development is aimed at a Pico attached to a VGA demonstration board as per
    chapter 3 of
    [Hardware design with RP2040](https://datasheets.raspberrypi.org/rp2040/hardware-design-with-rp2040.pdf)
    or the commercial version
    [Pimoroni Pico VGA Demo Base](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base).
    This is now also fairly advanced, although there are some issues to be resolved and
    more testing required.

This project includes source from various locations with difference licenses. See the
various LICENSE.txt files.

## Build Instructions (thanks to Eric)

To build this for the Pico, make sure you have the SDK installed and the
tinyusb module installed.

Ensure that the environment variable PICO_SDK_PATH points to the path where the SDK is installed.
Then type

     $ mkdir build
     $ cd build
     $ cmake [options] -S ../console/pico -B .
     $ make

Note the stop (current folder) after the -B flag.

The following options may be specified with the cmake command:

* -DPICO_BOARD=vgaboard if using the VGA demo board, or other board as appropriate.
* -DSTDIO=... to select the user interface
  * -DSTDIO=USB for the basic console on USB
  * -DSTDIO=UART for the basic console on UART.
  * -DSTDIO=PICO for input via USB keyboard and output via VGA monitor. With this option
    video generation is disabled while saving data to Flash. Video unaffected if saving
    to SD card.
  * -DSTDIO=PICO2 DO NOT USE! Input via USB keyboard and output via VGA monitor. This
    is supposed to continue to generate video while saving to Flash. However to to an
    unresolved issue saving to Flash while video is running results in random crashes.
* -DLFS=Y to include storage on Pico flash or -DLFS=N to exclude it.
* -DFAT=Y to include storage on SD card or -DFAT=N to exclude it.
* Other cmake options if required.

At this point the file bbcbasic.uf2 should be in the build directory.

Plug a Pico into the USB port while holding the boot button and then assuming developing
on a Raspberry Pi with Raspberry Pi OS:

     $ cp -v bbcbasic.uf2 /media/pi/RPI-RP2

If using USB, connect as

      $ picocom /dev/ttyACM0

Or if using Raspberry Pi serial port, connect as

      $ picocom -b 115200 /dev/ttyS0

to use the interpreter.  Note that you may use minicom as well, however,
minicom will not display color correctly or resize the terminal window
for the different MODE settings in BBC Basic.

The following limitations are noted:

1.  There is no assembler.  Something might appear to happen, but
    it will surely crash immediately afterwards.

2.  HIMEM-PAGE=65K with an additional 32K has reserved for CALL and
    INSTALL libraries.

3.  m0FaultDispatch is linked in by default.  This license for this
    library is free for hobby and other non-commercial products.  If
    you wish to create a commercial version of the program contained
    here, please add -DFREE to the CMakeLists.txt file.

4.  All programs in tests and the root filesystem have been tested
    and appear to work.  However, any remaining bugs are more likely
    to be related to the Pico port.  As always this is open source
    with expressly no warranty provided.

5.  The *CD command has been modified to automatically change the
    built-in variable @dir$ as the Pico doesn't have an operating
    system from which the executable was launched.

6.  There is are known buffer overflows in the wrappers appearing in
    lfswrap.c which are triggered when a filename path grows to be
    greater than 256 characters.  Please don't do that.

## Usage Notes - Stand-Alone (USB keyboard and VGA Output)

The code has been designed for use with the VGA demonstration board.
To use (once the Pico has been programmed):

* Power should be applied to the micro-USB socket on the demo board.
* The keyboard should be connected to the USB socket on the Pico itself.
A USB to micro-USB adaptor will be required. The Pico is somewhat selective
as to which keyboards work, cheap keyboards may be better.
* The SD card is used in SPI mode, and Pico GPIOs 20 & 21 are used for
serial input and output. On the Pimoroni board, cut the links between
these GPIOs and SD_DAT1 & SD_DAT2. Optionally solder a 2x3 header in place
for a serial connection. From BBC Basic printer output is sent to serial.
It is currently also used for diagnostic output.
* An SD or SDHC card may be used, It should be formatted as FAT.

The implementation currently supports 16 video modes:

Mode | Colours |   Text  | Graphics  | Letterbox
-----|---------|---------|-----------|----------
   0 |     2   | 80 x 32 | 640 x 256 |     Y
   1 |     4   | 40 x 32 | 320 x 256 |     Y
   2 |    16   | 20 x 32 | 160 x 256 |     Y
   3 |     2   | 80 x 24 | 640 x 240 |
   4 |     2   | 40 x 32 | 320 x 256 |     Y
   5 |     4   | 20 x 32 | 160 x 256 |     Y
   6 |     2   | 40 x 24 | 320 x 240 |
   7 |     8   | 40 x 24 | Teletext  |
   8 |     2   | 80 x 30 | 640 x 480 |
   9 |     4   | 40 x 30 | 320 x 480 |
  10 |    16   | 20 x 30 | 160 x 480 |
  11 |     2   | 80 x 24 | 640 x 480 |
  12 |     2   | 40 x 30 | 320 x 480 |
  13 |     4   | 20 x 30 | 120 x 480 |
  14 |     2   | 40 x 24 | 320 x 480 |
  15 |    16   | 40 x 24 | 320 x 240 |

Modes 0-2, 4 & 5 reproduce those from the BBC Micro. They only have 256 rows of pixels
which are displayed in the centre of the monitor so may appear squashed.

Modes 3 & 6 only have 24 lines of text compared to 25 on the BBC Micro (this may be changed),
however they can also display graphics.

Except for Mode 7, colours 8-15 are high intensity rather than flashing.

### Missing features & Qwerks

The Pico implementation is missing features compared to the BBCSDL implementation
on full operating systems. The limitations include:

* High resolution text (VDU 5) is implemented, but it does not scroll, instead wraps
around back to the top of the screen. New text overlays any existing rather than
replacing it.
* VDU 23 can only be used to control the appearance of the cursor and to select
page or scroll mode.
* PLOT modes 0-167 & 192-207 are implemented.
* There is no sound
* VGA output is lost while writing programs or data to internal flash memory.
This does not happen writing to SD card.

## TO DO - For the second development line

* Sort out the memory map for this mode - DONE but needs revisiting
* Enhance text processing (viewports) - DONE
* Implement teletext mode - DONE
* Implement graphics - DONE
* Improved keyboard processing - DONE
* High resolution text - DONE?
* Implement GET(X,Y) - DONE
* Plotting: fill, circle, ellipse - DONE
* Plotting: arc - DONE
* Plotting: segment & sector - Probaby will not do, too many cases. Can sometimes be achieved with arc & fill.
* 800x600 VGA output (currently only 640x480)?
* Sound?
* Testing - Lots of it
* Documentation
