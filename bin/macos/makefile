# Download pre-compiled frameworks to /Library/Frameworks/
# SDL2     from https://www.libsdl.org/download-2.0.php
# SDL2_ttf from https://www.libsdl.org/projects/SDL_ttf/
# SDL2_net from https://www.libsdl.org/projects/SDL_net/

SDL_LIB = -L/usr/include -ldl -lm

VPATH = ../../src ../../include
CXX = gcc -Wall -mmacosx-version-min=10.6 -I ../../include

OBJ = bbmain.o bbexec.o bbeval.o bbcmos.o bbccli.o \
      bbcvdu.o bbcvtx.o flood.o bbdata.o bbcsdl.o \
      bbasmb.o bbctmp.o

all: bbcsdl

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbasmb.o: bbasmb_x86_64.c BBC.h
	$(CXX) -c -Os $< -o $@

bbcmos.o: bbcmos.c bbcsdl.h
	$(CXX) -c -msse2 -O2 $< -o $@

bbccli.o: bbccli.c bbcsdl.h
	$(CXX) -Wno-unused-result -c -Os $< -o $@

bbcvdu.o: bbcvdu.c bbcsdl.h
	$(CXX) -c -Os $< -o $@

bbcvtx.o: bbcvtx.c bbcsdl.h
	$(CXX) -c -O3 $< -o $@

bbcsdl.o: bbcsdl.c bbcsdl.h version.h
	$(CXX) -Wno-unused-result -c -O2 $< -o $@

bbctmp.o: bbctmp.m
	$(CXX) -c -Os $< -o $@

SDL2_gfxPrimitives.o: SDL2_gfxPrimitives.c
	$(CXX) -c -O2 $< -o $@

SDL2_rotozoom.o: SDL2_rotozoom.c SDL_stbimage.h stb_image.h dr_mp3.h
	$(CXX) -c -O2 -fPIC $< -o $@

flood.o: flood.c
	$(CXX) -c -O3 $< -o $@

bbdata.o: ../../src/bbdata_x86_64.nas
	nasm --prefix _ -f macho64 -s ../../src/bbdata_x86_64.nas -o bbdata.o

libstb.dylib: SDL2_rotozoom.o SDL2_gfxPrimitives.o
	$(CXX) -dynamiclib -F/Library/Frameworks -framework SDL2 \
	-o libstb.dylib SDL2_rotozoom.o SDL2_gfxPrimitives.o

bbcsdl: $(OBJ) libstb.dylib
	$(CXX) $(OBJ) -lstb -F/Library/Frameworks -L . \
	-framework SDL2 -framework SDL2_ttf -framework SDL2_net \
	-framework Foundation \
	-o bbcsdl $(SDL_LIB)
	cp bbcsdl ../../
	cp libstb.dylib ../../
