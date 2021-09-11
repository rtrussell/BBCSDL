SOURCES=../../mcu/*.[chi] ../../src/*.c ../../include/*.h

all: usb uart filesystem
usb: buildusb/bbcbasic.uf2
uart: builduart/bbcbasic.uf2
filesystem:	../../mcu/lfsutil/filesystem.uf2

buildusb/CMakeCache.txt: ../../console/pico/CMakeLists.txt
	rm -rf buildusb
	mkdir -p buildusb
	cd buildusb && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake ../../../console/pico -DSTDIO=USB -DLFS=Y -DFAT=N

builduart/CMakeCache.txt: ../../console/pico/CMakeLists.txt
	rm -rf builduart
	mkdir -p builduart
	cd builduart && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake ../../../console/pico -DSTDIO=UART -DLFS=Y -DFAT=Y

buildusb/bbcbasic.uf2: buildusb/CMakeCache.txt $(SOURCES)
	cd buildusb && make
	cp buildusb/bbcbasic.uf2 ../../bbcbasicusb.uf2

builduart/bbcbasic.uf2: builduart/CMakeCache.txt $(SOURCES)
	cd builduart && make
	cp builduart/bbcbasic.uf2 ../../bbcbasicuart.uf2

../../mcu/sympico.i: ../../mcu/sympico.sh
	cd ../../mcu && ./sympico.sh

../../mcu/lfsutil/filesystem.uf2:
	cd ../../mcu/lfsutil && make
	cp ../../mcu/lfsutil/filesystem.uf2 ../..

clean:
	rm -rf buildusb builduart
	rm -f ../../bbcbasicusb.uf2 ../../bbcbasicuart.uf2 \
		../../filesystem.uf2
	cd ../../mcu/lfsutil && make clean
