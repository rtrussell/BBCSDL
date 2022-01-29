BOARD=vgaboard
MIN_STACK=N
SOUND=I2S
SOURCES= \
    ../../BBCSDL/src/bbmain.c \
    ../../BBCSDL/src/bbdata_arm_32.s \
	../../BBCSDL/include/BBC.h \
	../../BBCSDL/include/version.h \
	../../BBCSDL/include/bbccon.h \
    ../../BBCSDL/src/bbexec.c \
    ../../BBCSDL/src/bbeval.c \
    ../../src/bbexec2.c \
    ../../src/bbeval2.c \
    ../../src/bbccos.c \
    ../../src/bbpico.c \
    ../../src/bbasmb_arm_v6m.c \
	../../src/fbufvdu.c \
	../../src/sn76489.c \
    ../../src/lfswrap.c \
    ../../src/fault.c \
    ../../src/pico/sympico.c \
	../../src/pico/sympico.i \
	../../src/pico/pico_snd.c \
	../../src/pico/stack_trap.c \
	../../src/pico/ff_disk.c \
	../../src/pico/sd_spi2.c \
	../../src/pico/sd_spi.pio \
	../../src/pico/sound.pio \
	../../src/pico_gui/picokbd.c \
	../../src/pico_gui/picofbuf.c \
	../../src/pico_gui/framebuffer.S \
	../../src/pico_gui/gui_pico.ld \
	../../include/lfsmcu.h \
	../../include/ffconf.h \
	../../include/lfswrap.h \
	../../include/sconfig.h \
	../../include/bbuart.h \
    ../../littlefs/lfs.c \
    ../../littlefs/lfs_util.c \
    ../../littlefs/lfs.h \
    ../../littlefs/lfs_util.h \
	../../fatfs/ff.c \
	../../fatfs/ffsystem.c \
	../../fatfs/ffunicode.c \
	../../fatfs/ff.h \
	../../fatfs/diskio.h \
    ../../m0FaultDispatch/m0FaultDispatch.c

all: bbcbasic filesystem
bbcbasic: bbcbasic.uf2
filesystem:	filesystem.uf2

build/Makefile: ../../src/pico/CMakeLists.txt Makefile
	rm -rf build
	mkdir -p build
	cd build && PICO_SDK_PATH=$(PICO_SDK_PATH) \
		cmake -DPICO_BOARD=$(BOARD) -DSTDIO=PICO -DLFS=Y -DFAT=Y -DSOUND=$(SOUND) -DSTACK_CHECK=4 \
			-DMIN_STACK=$(MIN_STACK) -S ../../../src/pico -B .

../../BBCSDL/include/version.h:
	git submodule update --remote

../../littlefs/lfs.c:
	git submodule update --remote

bbcbasic.uf2: build/Makefile $(SOURCES)
	cd build && make
	cp build/bbcbasic.uf2 bbcbasic.uf2
	cp build/bbcbasic.elf bbcbasic.elf

../../src/pico/sympico.i: ../../src/pico/sympico.sh
	cd ../../src/pico && ./sympico.sh

filesystem.uf2:
	cd ../../src/lfsutil && make clean
	cd ../../src/lfsutil && make EXTRA=../../bin/pico/examples
	cp ../../src/lfsutil/filesystem.uf2 ../../bin/pico
	cp ../../src/lfsutil/filesystem.elf ../../bin/pico

clean:
	rm -rf build
	rm -f bbcbasic.* filesystem.*
	cd ../../src/lfsutil && make clean
