VPATH = ../../src ../../include ../../../BBCSDL/src ../../../BBCSDL/include
CXX = gcc -Wall -I ../../include -I ../../../BBCSDL/include

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o bbccos.o bbccon.o

all: bbcbasic

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 -ffast-math -fno-finite-math-only $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 -ffast-math -fno-finite-math-only $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds -c -O2 -ffast-math -fmath-errno -fno-finite-math-only $< -o $@

bbasmb.o: bbasmb_arm_64.c BBC.h
	$(CXX) -c -Os $< -o $@

bbccos.o: bbccos.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result -c -Os $< -o $@

bbccon.o: bbccon.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result -c -Os $< -o $@

bbdata.o: ../../../BBCSDL/src/bbdata_arm_64.s
	as ../../../BBCSDL/src/bbdata_arm_64.s -o bbdata.o

bbcbasic: $(OBJ)
	$(CXX) $(OBJ) -F/Library/Frameworks -L . \
	-framework Foundation \
	-o bbcbasic -L/usr/include -ldl -lm
	cp bbcbasic ../../
