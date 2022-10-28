CXX:=g++
CFLAG =  -Wall

SIMPLE:=np_simple
SIMPLE_IC:=$(SIMPLE)_dir/include
SIMPLE_SRC:=$(shell find np_simple_dir/src/ -name '*.cpp')
SIMPLE_OBJS:=$(SIMPLE_SRC:%.cpp=%.o) 

# SINGLE:=np_single_proc
# SINGLE_IC:=$(SINGLE)_dir/include
# SINGLE_SRC:=$(shell find np_single_proc_dir/src/ -name '*.cpp')
# SINGLE_OBJS:=$(SINGLE_SRC:%.cpp=%.o) 

all: $(SIMPLE)

$(SIMPLE):
	$(CXX) -o $@ -I$(SIMPLE_IC) $(CFLAG) $(SIMPLE_SRC)

# $(SINGLE):
# 	$(CXX) -o $@ -I$(SINGLE_IC) $(CFLAG) $(SINGLE_SRC)


	

ta: 
	$(CXX) commands/noop.cpp -o bin/noop
	$(CXX) commands/number.cpp -o bin/number
	$(CXX) commands/removetag.cpp -o bin/removetag
	$(CXX) commands/removetag0.cpp -o bin/removetag0
	$(CXX) commands/manyblessings.cpp -o bin/manyblessings


clean:
	$(RM) $(SIMPLE) $(SIMPLE_OBJS)