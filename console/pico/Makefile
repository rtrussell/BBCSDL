BOARD=pico
SOURCES= \
    ../../src/bbmain.c \
    ../../src/bbexec.c \
    ../../src/bbeval.c \
    ../../src/bbeval2.c \
    ../../src/bbccos.c \
    ../../src/bbdata_arm_32.s \
    ../../src/bbpico.c \
    ../../src/bbasmb_arm_v6m.c \
    ../../mcu/lfswrap.c \
    ../../mcu/m0FaultDispatch/m0FaultDispatch.c \
    ../../mcu/fault.c \
    ../../mcu/pico/sympico.c \
	../../mcu/pico/sympico.i \
    ../../mcu/pico/picoser.c \
	../../mcu/pico/stack_trap.c \
	../../include/BBC.h \
	../../include/version.h \
	../../include/bbccon.h \
	../../include/mcu/lfsmcu.h \
	../../include/mcu/ffconf.h \
	../../include/mcu/lfswrap.h \
	../../include/mcu/sconfig.h \
	../../include/mcu/bbuart.h \
	../../mcu/pico/bbc_pico.ld

all: bbcbasic filesystem
bbcbasic: bbcbasic.uf2
filesystem:	filesystem.uf2

build/Makefile: ../../mcu/pico/CMakeLists.txt
	rm -rf build
	mkdir -p build
	cd build && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DSTDIO=USB+UART -DLFS=Y -DFAT=N -DSTACK_CHECK=4 -S ../../../mcu/pico -B .

bbcbasic.uf2: build/Makefile $(SOURCES)
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
