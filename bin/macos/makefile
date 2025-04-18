# Download pre-compiled frameworks to /Library/Frameworks/
# SDL2     from https://www.libsdl.org/download-2.0.php
# SDL2_ttf from https://www.libsdl.org/projects/SDL_ttf/
# SDL2_net from https://www.libsdl.org/projects/SDL_net/

ARCH := $(shell uname -m)
SDL_LIB = -L/usr/include -ldl -lm
MIN_MACOS_VERSION = 10.7

VPATH = ../../src ../../include
CXX = gcc -Wall -pthread -mmacosx-version-min=$(MIN_MACOS_VERSION) -F /Library/Frameworks -I ../../include

ifeq ($(ARCH),arm64)
    OBJ_EXTRA = sort.o
endif

OBJ = bbmain.o bbexec.o bbeval.o bbcmos.o bbccli.o \
      bbcvdu.o bbcvtx.o flood.o bbdata.o bbcsdl.o \
      bbasmb.o bbctmp.o $(OBJ_EXTRA)

all: bbcsdl

clean:
	rm -f *.o bbcsdl libstb.dylib

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 -ffast-math -fno-finite-math-only $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 -ffast-math -fno-finite-math-only $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds -c -O2 -ffast-math -fmath-errno -fno-finite-math-only $< -o $@

bbcmos.o: bbcmos.c bbcsdl.h
	$(CXX) -Wno-unused-result -Wno-array-bounds -c -msse2 -O2 $< -o $@

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

SDL2_gfxPrimitives.o: SDL2_gfxPrimitives.c SDL2_gfxPrimitives.h
	$(CXX) -c -O2 $< -o $@

SDL2_rotozoom.o: SDL2_rotozoom.c SDL2_rotozoom.h SDL2_gfxPrimitives.h SDL_stbimage.h stb_image.h dr_mp3.h
	$(CXX) -c -Os -fPIC $< -o $@

flood.o: flood.c
	$(CXX) -c -O3 $< -o $@

sort.o: sort.c
	$(CXX) -c -O3 $< -o $@

ifeq ($(ARCH),arm64)
bbasmb.o: bbasmb_arm_64.c BBC.h
	$(CXX) -c -Os $< -o $@

    BBDATA_COMPILE = clang -mmacosx-version-min=$(MIN_MACOS_VERSION) -c
    BBDATA_SRC = ../../src/bbdata_arm_64.s
else 
bbasmb.o: bbasmb_x86_64.c BBC.h
	$(CXX) -c -Os $< -o $@

    BBDATA_COMPILE = nasm --prefix _ -f macho64 -s
    BBDATA_SRC = ../../src/bbdata_x86_64.nas
endif

bbdata.o: $(BBDATA_SRC)
	$(BBDATA_COMPILE) $(BBDATA_SRC) -o bbdata.o

libstb.dylib: SDL2_rotozoom.o SDL2_gfxPrimitives.o
	$(CXX) -dynamiclib -F/Library/Frameworks -framework SDL2 \
	-o libstb.dylib SDL2_rotozoom.o SDL2_gfxPrimitives.o

bbcsdl: $(OBJ) libstb.dylib
	$(CXX) $(OBJ) -lz -lstb -F/Library/Frameworks -L . \
	-framework SDL2 -framework SDL2_ttf -framework SDL2_net \
	-framework Foundation -Wl,-headerpad_max_install_names \
	-o bbcsdl $(SDL_LIB)
	install_name_tool -add_rpath /Library/Frameworks bbcsdl
	install_name_tool -change libstb.dylib "@executable_path/libstb.dylib" bbcsdl
	cp bbcsdl ../../
	cp libstb.dylib ../../
