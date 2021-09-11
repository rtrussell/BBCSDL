CFLAGS=-O2 -Wall -I.. -I../littlefs
TARGETS=filesystem.uf2 filesystem.lfs mklfsimage uf2conv
SOURCES=mklfsimage.c ../lfspico.c ../littlefs/lfs.a

all: $(TARGETS)

filesystem.uf2: filesystem.lfs uf2conv
	./uf2conv filesystem.lfs filesystem.uf2 0x10100000

tree:
	rm -rf tree
	mkdir tree
	mkdir tree/lib
	for i in arraylib.bbc classlib.bbc datelib.bbc fnusing.bbc \
		sortlib.bbc stringlib.bbc utf8lib.bbc xmllib.bbc; \
		do cp ../../lib/$$i tree/lib/$$i; done 
	cp ../../examples/general/calendar.bbc tree/
	cp ../../examples/general/sortdemo.bbc tree/
	for i in hanoi.bbc snake.bbc sudoku.bbc sudoku.dat; \
		do cp ../../examples/games/$$i tree/$$i; done 
	cp ../../examples/tools/timing.bbc tree/

filesystem.lfs: ../../lib/* mklfsimage tree
	rm -f filesystem.lfs
	./mklfsimage tree ../../examples/pico
	./mklfsimage -p tests ../../tests

mklfsimage: $(SOURCES)
	gcc $(CFLAGS) -o mklfsimage $(SOURCES)

../littlefs/lfs.a:
	cd ../littlefs && make

uf2conv: uf2conv.c uf2format.h
	gcc $(CFLAGS) -o uf2conv uf2conv.c uf2format.h

clean:
	rm -f $(TARGETS)
	rm -rf tree
