CFLAGS=-O2 -Wall -I.. -I../../littlefs -I../../include
TARGETS=filesystem.uf2 filesystem.elf filesystem.lfs mklfsimage uf2conv
SOURCES=mklfsimage.c ../lfsmcu.c ../../littlefs/lfs.a

all: $(TARGETS)

filesystem.uf2: filesystem.lfs uf2conv
	./uf2conv filesystem.lfs filesystem.uf2 0x10100000

filesystem.elf: filesystem.lfs
	arm-none-eabi-objcopy --change-addresses 0x10100000 --input-target=binary --output-target=elf32-little filesystem.lfs filesystem.elf

tree:
	rm -rf tree
	mkdir tree
	mkdir tree/lib
	for i in arraylib.bbc classlib.bbc datelib.bbc fnusing.bbc \
		sortlib.bbc stringlib.bbc utf8lib.bbc xmllib.bbc; \
		do cp -v ../../BBCSDL/lib/$$i tree/lib/$$i; done 
	cp -v ../../console/pico/examples/*.bbc tree/

filesystem.lfs: ../../BBCSDL/lib/* mklfsimage tree
	rm -f filesystem.lfs
	./mklfsimage tree ../../console/pico/examples
	./mklfsimage -p tests ../../BBCSDL/tests
ifdef EXTRA
	./mklfsimage $(EXTRA)
endif

mklfsimage: $(SOURCES)
	gcc $(CFLAGS) -o mklfsimage $(SOURCES)

../../littlefs/lfs.a:
	cd ../../littlefs && make

uf2conv: uf2conv.c uf2format.h
	gcc $(CFLAGS) -o uf2conv uf2conv.c uf2format.h

clean:
	rm -f $(TARGETS)
	rm -rf tree
