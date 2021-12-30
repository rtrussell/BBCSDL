# On Debian / Ubuntu / Mint download SDL 2.0 and NASM from the repository:
# sudo apt-get install libsdl2-dev
# sudo apt-get install libsdl2-ttf-dev
# sudo apt-get install libsdl2-net-dev
# sudo apt-get install libsdl2-2.0-0
# sudo apt-get install libsdl2-ttf-2.0-0
# sudo apt-get install libsdl2-net-2.0-0
# sudo apt-get install nasm

SDL_LIB = -L/usr/include -lSDL2 -lSDL2main -lSDL2_ttf -lSDL2_net -ldl -lm
LBITS := $(shell getconf LONG_BIT)

VPATH = ../../src ../../include
CXX = gcc -Wall -I ../../include

OBJ = bbmain.o bbexec.o bbeval.o bbcmos.o bbccli.o \
      bbcvdu.o bbcvtx.o flood.o bbdata.o bbcsdl.o \
      bbasmb.o SDL2_gfxPrimitives.o

all: bbcsdl

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 -freg-struct-return -ffast-math $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 -freg-struct-return -ffast-math $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds -c -O2 -freg-struct-return -ffast-math $< -o $@

bbasmb.o: bbasmb_x86_64.c BBC.h
	$(CXX) -c -Os $< -o $@

bbcmos.o: bbcmos.c bbcsdl.h
	$(CXX) -Wno-array-bounds -c -msse2 -O2 $< -o $@

bbccli.o: bbccli.c bbcsdl.h
	$(CXX) -Wno-unused-result -c -Os $< -o $@

bbcvdu.o: bbcvdu.c bbcsdl.h
	$(CXX) -c -Os $< -o $@

bbcvtx.o: bbcvtx.c bbcsdl.h
	$(CXX) -c -O3 $< -o $@

bbcsdl.o: bbcsdl.c bbcsdl.h version.h
	$(CXX) -Wno-unused-result -c -O2 $< -o $@

SDL2_gfxPrimitives.o: SDL2_gfxPrimitives.c SDL2_gfxPrimitives.h
	$(CXX) -c -O2 $< -o $@

SDL2_rotozoom.o: SDL2_rotozoom.c SDL2_rotozoom.h SDL2_gfxPrimitives.h SDL_stbimage.h stb_image.h dr_mp3.h
	$(CXX) -c -Os -fPIC $< -o $@

flood.o: flood.c
	$(CXX) -c -O3 $< -o $@

ifeq ($(LBITS),64)
bbdata.o: ../../src/bbdata_x86_64.nas
	nasm -f elf64 -s ../../src/bbdata_x86_64.nas -o bbdata.o
else
bbdata.o: ../../src/bbdata_x86_32.nas
	nasm -f elf32 -s ../../src/bbdata_x86_32.nas -o bbdata.o
endif

libstb.so: SDL2_rotozoom.o
	$(CXX) -shared -o libstb.so SDL2_rotozoom.o

bbcsdl: $(OBJ) libstb.so
	$(CXX) $(OBJ) -lstb -L . \
	-o bbcsdl $(SDL_LIB) -Wl,-s -Wl,-R,'$$ORIGIN'
	cp bbcsdl ../../
	cp libstb.so ../../
