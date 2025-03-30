ARCH := $(shell uname -m)
MIN_MACOS_VERSION = 10.7

VPATH = ../../src ../../include ../../../BBCSDL/src ../../../BBCSDL/include
CXX = gcc -Wall -mmacosx-version-min=$(MIN_MACOS_VERSION) -I ../../include -I ../../../BBCSDL/include

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o bbccos.o bbccon.o

all: bbcbasic

clean:
	rm -f *.o bbcbasic

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds -c -O2 $< -o $@

bbasmb.o: bbasmb_x86_64.c BBC.h
	$(CXX) -c -Os $< -o $@

bbccos.o: bbccos.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result -c -Os $< -o $@

bbccon.o: bbccon.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result -c -Os $< -o $@

ifeq ($(ARCH),arm64)
    BBDATA_COMPILE = clang -mmacosx-version-min=$(MIN_MACOS_VERSION) -c
    BBDATA_SRC = ../../../BBCSDL/src/bbdata_arm_64.s
else 
    BBDATA_COMPILE = nasm --prefix _ -f macho64 -s
    BBDATA_SRC = ../../../BBCSDL/src/bbdata_x86_64.nas
endif

bbdata.o: $(BBDATA_SRC)
	$(BBDATA_COMPILE) $(BBDATA_SRC) -o bbdata.o

bbcbasic: $(OBJ)
	$(CXX) $(OBJ) -F/Library/Frameworks -L . \
	-framework Foundation \
	-o bbcbasic -L/usr/include -ldl -lm
	cp bbcbasic ../../
