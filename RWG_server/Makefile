CXX:=g++
CFLAG =  -Wall

SIMPLE:=np_simple
SIMPLE_IC:=$(SIMPLE)_/include
SIMPLE_SRC:=$(shell find np_simple_/src/ -name '*.cpp')
SIMPLE_OBJS:=$(SIMPLE_SRC:%.cpp=%.o) 

SINGLE:=np_single_proc
SINGLE_IC:=$(SINGLE)_/include
SINGLE_SRC:=$(shell find np_single_proc_/src/ -name '*.cpp')
SINGLE_OBJS:=$(SINGLE_SRC:%.cpp=%.o) 

all: $(SINGLE)

$(SIMPLE):
	$(CXX) -o $@ -I$(SIMPLE_IC) $(CFLAG) $(SIMPLE_SRC)

$(SINGLE):
	$(CXX) -o $@ -I$(SINGLE_IC) $(CFLAG) $(SINGLE_SRC)

test : clean all
	./np_single_proc 7001
	

ta: 
	$(CXX) commands/noop.cpp -o bin/noop
	$(CXX) commands/number.cpp -o bin/number
	$(CXX) commands/removetag.cpp -o bin/removetag
	$(CXX) commands/removetag0.cpp -o bin/removetag0
	$(CXX) commands/manyblessings.cpp -o bin/manyblessings


clean:
	$(RM) $(SINGLE) $(SINGLE_OBJS)