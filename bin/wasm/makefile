# Requires Python and the Emscripten SDK to be installed.

VPATH  = ../BBCSDL/src ../BBCSDL/include

CXX = emcc -s USE_PTHREADS=1 -s USE_SDL=2 -s USE_SDL_TTF=2 -s USE_SDL_NET=2 \
      -I . -I ../BBCSDL/include -Wall -Wno-pthreads-mem-growth

LXX = emcc -s USE_PTHREADS=1 -s USE_SDL=2 -s USE_SDL_TTF=2 -s USE_SDL_NET=2 -s MAXIMUM_MEMORY=1gb \
      -s PTHREAD_POOL_SIZE=2 -s ALLOW_MEMORY_GROWTH=1 -s WASM=1 -s ASSERTIONS=0 -s STACK_SIZE=5MB \
      -I . -I ../BBCSDL/include -Wall -Wno-pthreads-mem-growth

OBJ = bbmain.o bbexec.o bbeval.o bbcmos.o bbccli.o \
      bbcvdu.o bbcvtx.o flood.o sort.o bbdata.o bbcsdl.o \
      bbasmb.o SDL2_gfxPrimitives.o SDL2_rotozoom.o

all: bbcsdl.html

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O3 -ffast-math -fno-finite-math-only $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O3 -ffast-math -fno-finite-math-only $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -c -O3 -ffast-math -fno-finite-math-only $< -o $@

bbasmb.o: bbasmb_wasm32.c BBC.h
	$(CXX) -c -Os $< -o $@

bbcmos.o: bbcmos.c bbcsdl.h
	$(CXX) -c -O3 $< -o $@

bbccli.o: bbccli.c bbcsdl.h
	$(CXX) -c -Os $< -o $@

bbcvdu.o: bbcvdu.c bbcsdl.h
	$(CXX) -c -Os $< -o $@

bbcvtx.o: bbcvtx.c bbcsdl.h
	$(CXX) -c -O3 $< -o $@

bbcsdl.o: bbcsdl.c bbcsdl.h version.h
	$(CXX) -Wno-unused-value -c -O3 $< -o $@

SDL2_gfxPrimitives.o: SDL2_gfxPrimitives.c SDL2_gfxPrimitives.h
	$(CXX) -c -O3 $< -o $@

SDL2_rotozoom.o: SDL2_rotozoom.c SDL2_rotozoom.h SDL2_gfxPrimitives.h SDL_stbimage.h stb_image.h
	$(CXX) -c -Os $< -o $@

flood.o: flood.c
	$(CXX) -c -O3 $< -o $@

sort.o: sort.c
	$(CXX) -c -O3 $< -o $@

bbdata.o: bbdata_wasm32.c
	$(CXX) -c $< -o $@

bbcsdl.html: $(OBJ) Box2D231.o bbcsdl_shell.html
	$(LXX) -O3 --emrun --no-heap-copy --shell-file bbcsdl_shell.html $(OBJ) Box2D231.o \
	-lidbfs.js -o bbcsdl.html --preload-file examples/ --preload-file lib/

index.html: $(OBJ) Box2D231.o bbcsdl_shell.html
	$(LXX) -O3 --emrun --no-heap-copy --shell-file bbcsdl_shell.html $(OBJ) Box2D231.o \
	-lidbfs.js -o index.html
