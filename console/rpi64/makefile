VPATH = ../../src ../../include ../../../BBCSDL/src ../../../BBCSDL/include
CXX = gcc -Wall -I ../../include -I ../../../BBCSDL/include

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o bbccos.o bbccon.o sort.o

all: bbcbasic

bbmain.o: bbmain.c BBC.h
	$(CXX) -Wno-attributes -c -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -Wno-array-bounds -c -O2 $< -o $@

bbasmb.o: bbasmb_arm_64.c BBC.h
	$(CXX) -c -Os $< -o $@

sort.o: sort.c
	$(CXX) -c -O3 $< -o $@

bbccos.o: bbccos.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result -c -Os $< -o $@

bbccon.o: bbccon.c bbccon.h
	$(CXX) -Wno-array-bounds -Wno-unused-result -c -O2 $< -o $@

bbdata.o: ../../../BBCSDL/src/bbdata_arm_64.s
	sed 's/_\([a-z]*\)/\1/g' <../../../BBCSDL/src/bbdata_arm_64.s >bbdata.s
	as bbdata.s -o bbdata.o

bbcbasic: $(OBJ)
	$(CXX) $(OBJ) -L . -L/usr/lib/ -ldl -lm -lrt -pthread \
	-o bbcbasic -Wl,-s -Wl,-R,'$$ORIGIN'
	cp bbcbasic ../../
