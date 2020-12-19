# Update these paths as necessary to match your installation
VPATH = ../../src ../../include ../../../BBCSDL/src ../../../BBCSDL/include
CXX = gcc -Wall -I ../../include -I ../../../BBCSDL/include

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o bbccos.o bbccon.o

all: bbcbasic.exe

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbasmb.o: bbasmb_x86_64.c BBC.h
	$(CXX) -c -Os $< -o $@

bbccos.o: bbccos.c bbccon.h
	$(CXX) -c -Os $< -o $@

bbccon.o: bbccon.c bbccon.h
	$(CXX) -Wno-unused-value -c -Os $< -o $@

bbdata.o: ../../../BBCSDL/src/bbdata_x86_64.nas
	nasm -f win64 -s ../../../BBCSDL/src/bbdata_x86_64.nas -o bbdata.o -l bbdata.lst

bbcbasic.exe: $(OBJ)
	$(CXX) $(OBJ) \
	-o bbcbasic.exe -lmingw32 -lwinmm -Wl,-s -Wl,--export-all-symbols
	copy bbcbasic.exe ..\..\
