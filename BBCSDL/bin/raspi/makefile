# Get SDL 2.0 from the Raspbian repository:
# sudo apt-get install libsdl2-dev
# sudo apt-get install libsdl2-ttf-dev
# sudo apt-get install libsdl2-net-dev
# sudo apt-get install libsdl2-2.0-0
# sudo apt-get install libsdl2-ttf-2.0-0
# sudo apt-get install libsdl2-net-2.0-0

SDL_LIB = -L/usr/lib/ -lSDL2 -lSDL2main -lSDL2_ttf -lSDL2_net -ldl -lm

VPATH = ../../src ../../include
CXX = gcc -Wall -I ../../include
CXXFLAGS = -march=armv7-a -mthumb -munaligned-access -mfloat-abi=hard -c

OBJ = bbmain.o bbexec.o bbeval.o bbcmos.o bbccli.o \
      bbcvdu.o bbcvtx.o flood.o bbdata.o bbcsdl.o \
      bbasmb.o SDL2_gfxPrimitives.o

all: bbcsdl

bbmain.o: bbmain.c BBC.h
	$(CXX) -Wno-attributes $(CXXFLAGS) -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) $(CXXFLAGS) -DARMHF -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds $(CXXFLAGS) -O2 $< -o $@

bbasmb.o: bbasmb_arm_32.c BBC.h
	$(CXX) $(CXXFLAGS) -Os $< -o $@

bbcmos.o: bbcmos.c bbcsdl.h
	$(CXX) -Wno-array-bounds $(CXXFLAGS) -mfpu=neon -O2 $< -o $@

bbccli.o: bbccli.c bbcsdl.h
	$(CXX) -Wno-unused-result $(CXXFLAGS) -Os $< -o $@

bbcvdu.o: bbcvdu.c bbcsdl.h
	$(CXX) $(CXXFLAGS) -DARMHF -Os $< -o $@

bbcvtx.o: bbcvtx.c bbcsdl.h
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

bbcsdl.o: bbcsdl.c bbcsdl.h version.h
	$(CXX) -Wno-unused-result $(CXXFLAGS) -O2 $< -o $@

SDL2_gfxPrimitives.o: SDL2_gfxPrimitives.c SDL2_gfxPrimitives.h
	$(CXX) $(CXXFLAGS) -O2 $< -o $@

SDL2_rotozoom.o: SDL2_rotozoom.c SDL2_rotozoom.h SDL2_gfxPrimitives.h SDL_stbimage.h stb_image.h dr_mp3.h
	$(CXX) $(CXXFLAGS) -Os -fPIC $< -o $@

flood.o: flood.c
	$(CXX) $(CXXFLAGS) -O3 $< -o $@

bbdata.o: ../../src/bbdata_arm_32.s
	as ../../src/bbdata_arm_32.s -o bbdata.o

libstb.so: SDL2_rotozoom.o
	$(CXX) -shared -o libstb.so SDL2_rotozoom.o

bbcsdl: $(OBJ) libstb.so
	$(CXX) $(OBJ) -lstb -L . \
	$(SDL_LIB) -o bbcsdl -Wl,-s -Wl,-R,'$$ORIGIN'
	cp bbcsdl ../../
	cp libstb.so ../../
