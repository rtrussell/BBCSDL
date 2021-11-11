BOARD=vgaboard
SOURCES= \
    ../../src/bbmain.c \
    ../../src/bbexec.c \
    ../../src/bbexec2.c \
    ../../src/bbeval.c \
    ../../src/bbeval2.c \
    ../../src/bbccos.c \
    ../../src/bbdata_arm_32.s \
    ../../src/bbpico.c \
    ../../src/bbasmb_arm_v6m.c \
	../../src/fbufvdu.c \
	../../src/sn76489.c \
    ../../mcu/lfswrap.c \
    ../../mcu/m0FaultDispatch/m0FaultDispatch.c \
    ../../mcu/fault.c \
    ../../mcu/pico/sympico.c \
	../../mcu/pico/sympico.i \
	../../mcu/pico/pico_snd.c \
	../../mcu/pico/stack_trap.c \
	../../mcu/fatfs/ff.c \
	../../mcu/fatfs/ffsystem.c \
	../../mcu/fatfs/ffunicode.c \
	../../mcu/pico/ff_disk.c \
	../../mcu/pico/sd_spi2.c \
	../../mcu/pico_gui/picokbd.c \
	../../mcu/pico_gui/picofbuf.c \
	../../mcu/pico_gui/framebuffer.S \
	../../include/BBC.h \
	../../include/version.h \
	../../include/bbccon.h \
	../../include/mcu/lfsmcu.h \
	../../include/mcu/ffconf.h \
	../../include/mcu/lfswrap.h \
	../../include/mcu/sconfig.h \
	../../include/mcu/bbuart.h \
	../../mcu/fatfs/ff.h \
	../../mcu/fatfs/diskio.h \
	../../mcu/pico/sd_spi.pio \
	../../mcu/pico/sound.pio \
	../../mcu/pico_gui/gui_pico.ld

all: bbcbasic filesystem
bbcbasic: bbcbasic.uf2
filesystem:	filesystem.uf2

build/Makefile: ../../mcu/pico/CMakeLists.txt Makefile
	rm -rf build
	mkdir -p build
	cd build && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DSTDIO=PICO -DLFS=Y -DFAT=Y -DSOUND=Y -DSTACK_CHECK=4 \
			-DMIN_STACK=Y -S ../../../mcu/pico -B .

bbcbasic.uf2: build/Makefile $(SOURCES)
	cd build && make
	cp build/bbcbasic.uf2 bbcbasic.uf2
	cp build/bbcbasic.elf bbcbasic.elf

../../mcu/pico/sympico.i: ../../mcu/pico/sympico.sh
	cd ../../mcu/pico && ./sympico.sh

filesystem.uf2:
	cd ../../mcu/lfsutil && make clean
	cd ../../mcu/lfsutil && make EXTRA=../../bin/pico/examples
	cp ../../mcu/lfsutil/filesystem.uf2 ../../bin/pico
	cp ../../mcu/lfsutil/filesystem.elf ../../bin/pico

clean:
	rm -rf build
	rm -f bbcbasic.* filesystem.*
	cd ../../mcu/lfsutil && make clean
