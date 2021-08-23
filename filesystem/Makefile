CFLAGS=-O2 -Wall -I../include -I../littlefs
TARGETS=filesystem.uf2 filesystem.lfs mklfsimage uf2conv
SOURCES=mklfsimage.c ../src/lfspico.c ../littlefs/lfs.a

all: $(TARGETS)

filesystem.uf2: filesystem.lfs uf2conv
	./uf2conv filesystem.lfs filesystem.uf2 0x10100000

filesystem.lfs: ../lib/* mklfsimage bbcbasic_console_linux.zip
	rm -rf tree
	mkdir tree
	cd tree && unzip ../bbcbasic_console_linux.zip
	rm tree/bbcbasic
	rm -f filesystem.lfs
	./mklfsimage tree extra
	./mklfsimage -p tests ../tests

mklfsimage: $(SOURCES)
	gcc $(CFLAGS) -o mklfsimage $(SOURCES)

../littlefs/lfs.a:
	cd ../littlefs && make

uf2conv: uf2conv.c uf2format.h
	gcc $(CFLAGS) -o uf2conv uf2conv.c uf2format.h

clean:
	rm -f $(TARGETS)
	rm -rf tree
