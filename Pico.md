# BBC BASIC for the Raspberry Pi Pico

## Description

This is a work in progress and is known to be incomplete, for example the embedded assembler
cannot yet be used because it's for the ARM-32 instruction set rather than the subset of the
Thumb2 instruction set used by the Pico's Cortex-M0+ CPU.

It includes contributions from:

* R. T. Russell
* Eric Olson
* Memotech-Bill

## Installation

To install BBC BASIC on a Raspberry Pi Pico proceed as follows (these instructions are for
running on a Raspberry Pi but should be adaptable for other platforms):

1. Download **bbcbasic.uf2** from http://www.rtr.myzen.co.uk/bbcbasic.uf2

2. Plug a Pico into the USB port while holding the boot button; this should result in the
Pico appearing as an external drive.  On a Raspberry Pi it will be at /media/pi/RPI-RP2

3. Copy the file bbcbasic.uf2 to the Pico:

      $ cp -v bbcbasic.uf2 /media/pi/RPI-RP2

Please note that if you repeat the installation you will delete any files that you have 
saved to the local filesystem on the Pico.

## Operation

This is a Console Mode build of BBC BASIC and requires an external terminal; it is
configured to use the USB port for the terminal connection.  On a Raspberry Pi connect as:

      $ picocom /dev/ttyACM0

Note that you may alternatively use **minicom**, however it will not display colour
correctly or resize the terminal window for the different MODE settings in BBC BASIC.

