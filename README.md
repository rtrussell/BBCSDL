# BBCSDL
BBC BASIC for SDL 2.0 (BBCSDL) is a cross-platform implementation of the BBC BASIC programming language for
Windows, Linux (x86), MacOS, Raspbian (Raspberry Pi OS), Android, iOS and Emscripten / WebAssembly.
It is highly compatible with BBC BASIC for Windows and has the same language extensions, but uses
SDL 2.0 as an OS abstraction layer to achieve cross-platform compatibility.

The BBC BASIC Console Mode editions (BBCTTY) are lightweight implementations for Windows, Linux (x86),
MacOS and Raspbian (Raspberry Pi OS) which do not support graphics or sound but are otherwise 
compatible with the desktop, mobile and web editions.  They take their input from stdin and
send their output to stdout, so may be used for scripting, CGI and remote terminal applications.

![Architecture](https://www.bbcbasic.co.uk/bbcsdl/arch.png)

The files in green constitute the generic BBC BASIC interpreter which is shared by all the
editions.  The files in the red box are used to build the Console Mode editions.  The files in
the blue box are used to build the SDL 2.0 editions.  The files in brown run in the GUI (main)
thread, all the others run in the interpreter thread.

The files with the 1 and 2 superscripts are CPU-specific and the different variants are listed
beneath (not all exist!).  Note that bbasmb_wasm32.c isn't an assembler, but has been used as a
convenient place to put the function wrappers needed to support SYS in the in-browser edition.

Not indicated in the diagram is that the in-browser edition uses different versions of bbc.h
and bbcsdl.h from the rest.

Note that the name 'BBC BASIC' is used by permission of the British Broadcasting Corporation
and is not transferrable to a derived or forked work.
