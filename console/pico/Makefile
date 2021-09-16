SOURCES=../../mcu/*.[chi] ../../src/*.c ../../include/*.h

all: usb uart filesystem
usb: buildusb/bbcbasic.uf2
uart: builduart/bbcbasic.uf2
filesystem:	../../mcu/lfsutil/filesystem.uf2

buildusb/CMakeCache.txt: CMakeLists.txt
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

builduart/CMakeCache.txt: CMakeLists.txt $(FATFS_SOURCES)
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
