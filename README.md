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
    This is still in an early phase, requiring much more work and may often be broken.

This project includes source from various locations with difference licenses. See the
various LICENSE.txt files.

## Instructions (thanks to Eric)

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
* -DSTDIO=USB for the basic console on USB or -DSTDIO=UART for the basic console on UART.
  Use -DSTDIO=PICO for input via USB keyboard and output via VGA monitor.
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

## TO DO - For the second development line

* Sort out the memory map for this mode - DONE but needs revisiting
* Enhance text processing (viewports) - DONE
* Implement teletext mode - DONE
* Implement graphics - DONE
* Improved keyboard processing - DONE
* High resolution text - DONE?
* Implement GET(X,Y) - DONE
* Plotting: fill, circle, ellipse - DONE
* Plotting: arc - Maybe ?
* Plotting: segment & sector - Probaby will not do, too many cases. Can sometimes be achieved with arc & fill.
* 800x600 VGA output (currently only 640x480)?
* Sound?
* Testing - Lots of it
* Documentation
