VPATH = ../../src ../../include ../../../BBCSDL/src ../../../BBCSDL/include
CXX = gcc -Wall -mmacosx-version-min=10.6 -I ../../include -I ../../../BBCSDL/include

OBJ = bbmain.o bbexec.o bbeval.o bbasmb.o bbdata.o bbccos.o bbccon.o

all: bbcbasic

bbmain.o: bbmain.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbexec.o: bbexec.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbeval.o: bbeval.c BBC.h
	$(CXX) -c -O2 $< -o $@

bbasmb.o: bbasmb_x86_64.c BBC.h
	$(CXX) -c -Os $< -o $@

bbccos.o: bbccos.c bbccon.h
	$(CXX) -Wno-unused-result -c -Os $< -o $@

bbccon.o: bbccon.c bbccon.h
	$(CXX) -Wno-unused-result -c -Os $< -o $@

bbdata.o: ../../../BBCSDL/src/bbdata_x86_64.nas
	nasm --prefix _ -f macho64 -s ../../../BBCSDL/src/bbdata_x86_64.nas -o bbdata.o

bbcbasic: $(OBJ)
	$(CXX) $(OBJ) -F/Library/Frameworks -L . \
	-framework Foundation \
	-o bbcbasic -L/usr/include -ldl -lm
	cp bbcbasic ../../
