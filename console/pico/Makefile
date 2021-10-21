BOARD=pico
SOURCES= \
    ../../src/bbmain.c \
    ../../src/bbexec.c \
    ../../src/bbeval.c \
    ../../src/bbccos.c \
    ../../src/bbdata_arm_32.s \
    ../../src/bbpico.c \
    ../../src/bbasmb_arm_v6m2.c \
    ../../mcu/lfswrap.c \
    ../../mcu/m0FaultDispatch/m0FaultDispatch.c \
    ../../mcu/fault.c \
    ../../mcu/pico/sympico.c \
	../../mcu/pico/sympico.i \
	../../include/BBC.h \
	../../include/version.h \
	../../include/mcu/lfsmcu.h \
	../../include/mcu/ffconf.h \
	../../include/mcu/lfswrap.h \
	../../include/bbccon.h \
	../../mcu/pico/bbc_pico.ld

all: bbcbasic filesystem
bbcbasic: bbcbasic.uf2
filesystem:	filesystem.uf2

build/CMakeCache.txt: ../../mcu/pico/CMakeLists.txt
	rm -rf build
	mkdir -p build
	cd build && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DSTDIO=USB+UART -DLFS=Y -DFAT=N -S ../../../mcu/pico -B .

bbcbasic.uf2: build/CMakeCache.txt $(SOURCES)
	cd build && make
	cp build/bbcbasic.uf2 bbcbasic.uf2
	cp build/bbcbasic.elf bbcbasic.elf

../../mcu/pico/sympico.i: ../../mcu/pico/sympico.sh
	cd ../../mcu/pico && ./sympico.sh

filesystem.uf2:
	cd ../../mcu/lfsutil && make clean
	cd ../../mcu/lfsutil && make
	cp ../../mcu/lfsutil/filesystem.uf2 ../../console/pico
	cp ../../mcu/lfsutil/filesystem.elf ../../console/pico

clean:
	rm -rf build
	rm -f bbcbasic.* filesystem.*
	cd ../../mcu/lfsutil && make clean
