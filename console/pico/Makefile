SOURCES=../../mcu/*.[chi] ../../src/*.c ../../include/*.h

all: usb uart filesystem
usb: buildusb/bbcbasic.uf2
uart: builduart/bbcbasic.uf2
filesystem:	../../mcu/lfsutil/filesystem.uf2

buildusb/CMakeCache.txt: CMakeLists.txt ../../mcu/littlefs/.git
	rm -rf buildusb
	mkdir -p buildusb
	cd buildusb && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake .. -DSTDIO=USB -DLFS=Y -DFAT=N

FATFS_SOURCES= \
	../../mcu/fatfs/ff.c \
	../../mcu/fatfs/ff.h \
	../../mcu/fatfs/ffsystem.c \
	../../mcu/fatfs/ffunicode.c \
	../../mcu/fatfs/diskio.h \
	../../mcu/ffconf.h

# GCC always searches the source file's directory first for quoted includes.
# Unfortunately fatfs uses quoted include #include "ffconf.h"
# This prevents us from overriding the default fatfs configuration
# without modifying it as part of the fatfs submodule. We don't want to
# do that. Instead this kludge. Create symbolic links elsewhare to those
# fatfs source files we want to use. In the same folder we have our
# customized ffconf.h file.

../../mcu/fatfs/ff.c:
	ln -s ../../mcu/fatfs/source/ff.c ../../mcu/fatfs/ff.c

../../mcu/fatfs/ff.h:
	ln -s ../../mcu/fatfs/source/ff.h ../../mcu/fatfs/ff.h

../../mcu/fatfs/ffsystem.c:
	ln -s ../../mcu/fatfs/source/ffsystem.c ../../mcu/fatfs/ffsystem.c

../../mcu/fatfs/ffunicode.c:
	ln -s ../../mcu/fatfs/source/ffunicode.c ../../mcu/fatfs/ffunicode.c

../../mcu/fatfs/diskio.h:
	ln -s ../../mcu/fatfs/source/diskio.h ../../mcu/fatfs/diskio.h


builduart/CMakeCache.txt: CMakeLists.txt $(FATFS_SOURCES) ../../mcu/littlefs/.git ../../mcu/fatfs/.git
	rm -rf builduart
	mkdir -p builduart
	cd builduart && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake .. -DSTDIO=UART -DLFS=Y -DFAT=Y

buildusb/bbcbasic.uf2: buildusb/CMakeCache.txt $(SOURCES)
	cd buildusb && make
	cp buildusb/bbcbasic.uf2 bbcbasicusb.uf2
	cp buildusb/bbcbasic.elf bbcbasicusb.elf

builduart/bbcbasic.uf2: builduart/CMakeCache.txt $(SOURCES)
	cd builduart && make
	cp builduart/bbcbasic.uf2 bbcbasicuart.uf2
	cp builduart/bbcbasic.elf bbcbasicuart.elf

../../mcu/sympico.i: ../../mcu/sympico.sh
	cd ../../mcu && ./sympico.sh

../../mcu/lfsutil/filesystem.uf2:
	cd ../../mcu/lfsutil && make
	cp ../../mcu/lfsutil/filesystem.uf2 ../../console/pico
	cp ../../mcu/lfsutil/filesystem.elf ../../console/pico

clean:
	rm -rf buildusb builduart
	rm -f bbcbasicusb.* bbcbasicuart.* filesystem.*
	cd ../../mcu/lfsutil && make clean
