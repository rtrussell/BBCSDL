# Update these paths as necessary to match your installation
SDL_LIB = -LC:/SDL2-2.26.2/i686-w64-mingw32/lib -lSDL2main -lSDL2 \
          -LC:/SDL2_ttf-2.20.2/i686-w64-mingw32/lib -lSDL2_ttf \
          -LC:/SDL2_net-2.2.0/i686-w64-mingw32/lib -lSDL2_net 
SDL_INCLUDE = -IC:/SDL2-2.26.2/i686-w64-mingw32/include/ \
          -IC:/SDL2_ttf-2.20.2/i686-w64-mingw32/include/ \
          -IC:/SDL2_net-2.2.0/i686-w64-mingw32/include/ 

VPATH = ../../src ../../include
CXX = gcc -Wall -I ../../include

OBJ = bbmain.o bbexec.o bbeval.o bbcmos.o bbccli.o \
      bbcvdu.o bbcvtx.o flood.o bbdata.o bbcsdl.o \
      bbasmb.o SDL2_gfxPrimitives.o SDL2_rotozoom.o

all: bbcsdl.exe sdlrun.exe

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 -freg-struct-return -ffast-math -fno-finite-math-only $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 -freg-struct-return -ffast-math -fno-finite-math-only $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds -c -O2 -freg-struct-return -ffast-math -fmath-errno -fno-finite-math-only $< -o $@

# TODO: Need bbasmb_x86_32 assembler!!
bbasmb.o: bbasmb_arm_v6m.c BBC.h
	$(CXX) -c -Os $< -o $@

bbcmos.o: bbcmos.c bbcsdl.h
	$(CXX) -Wno-array-bounds -c $(SDL_INCLUDE) -msse2 -O2 $< -o $@

bbccli.o: bbccli.c bbcsdl.h
	$(CXX) -c $(SDL_INCLUDE) -Os $< -o $@

bbcvdu.o: bbcvdu.c bbcsdl.h
	$(CXX) -c $(SDL_INCLUDE) -Os $< -o $@

bbcvtx.o: bbcvtx.c bbcsdl.h
	$(CXX) -c $(SDL_INCLUDE) -O3 $< -o $@

bbcsdl.o: bbcsdl.c bbcsdl.h version.h
	$(CXX) -c -Wno-unused-value $(SDL_INCLUDE) -O2 $< -o $@

SDL2_gfxPrimitives.o: SDL2_gfxPrimitives.c SDL2_gfxPrimitives.h
	$(CXX) -c $(SDL_INCLUDE) -O2 $< -o $@

SDL2_rotozoom.o: SDL2_rotozoom.c SDL2_rotozoom.h SDL2_gfxPrimitives.h SDL_stbimage.h stb_image.h dr_mp3.h
	$(CXX) -c $(SDL_INCLUDE) -Os $< -o $@

flood.o: flood.c
	$(CXX) -c $(SDL_INCLUDE) -O3 $< -o $@

bbdata.o: ../../src/bbdata_x86_32.nas
	nasm --prefix _ -f win32 -s ../../src/bbdata_x86_32.nas -o bbdata.o

bbcsdl.exe: $(OBJ)
	$(CXX) $(OBJ) \
	-o bbcsdl.exe -lz -lmingw32 -lpsapi -mthreads -mwindows $(SDL_LIB) -Wl,-s -Wl,--export-all-symbols

sdlrun.o: sdlrun.c
	$(CXX) -Wall -c -O2 $< -o $@

unpack32.o: unpack32.nas
	nasm --prefix _ -f win32 -s unpack32.nas -o unpack32.o

sdlrun.exe: sdlrun.o unpack32.o
	$(CXX) sdlrun.o unpack32.o -o sdlrun.exe -lmingw32 -mwindows -Wl,-s 
