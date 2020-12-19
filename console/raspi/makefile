VPATH = ../../src ../../include ../../../BBCSDL/src ../../../BBCSDL/include
CXX = gcc -Wall -I ../../include -I ../../../BBCSDL/include
CXXFLAGS = -march=armv7-a -mthumb -munaligned-access -mfloat-abi=hard -c

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o bbccos.o bbccon.o

all: bbcbasic

bbmain.o: bbmain.c BBC.h
	$(CXX) -Wno-attributes $(CXXFLAGS) -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) $(CXXFLAGS) -DARMHF -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) $(CXXFLAGS) -O2 $< -o $@

bbasmb.o: bbasmb_arm_32.c BBC.h
	$(CXX) $(CXXFLAGS) -Os $< -o $@

bbccos.o: bbccos.c bbccon.h
	$(CXX) -Wno-unused-result $(CXXFLAGS) -Os $< -o $@

bbccon.o: bbccon.c bbccon.h
	$(CXX) -Wno-unused-result $(CXXFLAGS) -O2 $< -o $@

bbdata.o: ../../../BBCSDL/src/bbdata_arm_32.s
	as ../../../BBCSDL/src/bbdata_arm_32.s -o bbdata.o

bbcbasic: $(OBJ)
	$(CXX) $(OBJ) -L . -L/usr/lib/ -ldl -lm -lrt -pthread \
	-o bbcbasic -Wl,-s -Wl,-R,'$$ORIGIN'
	cp bbcbasic ../../
