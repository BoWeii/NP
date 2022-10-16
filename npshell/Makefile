CXX:=g++
TARGET:=npshell
CFLAG = -Iinclude -Wall
SRC:=$(shell find src/ -name '*.cpp')

SRC_OBJS:=$(SRC:%.cpp=%.o) 
%.o: %.cpp
	$(CXX) -o $@ $(CFLAG) -c $<




$(TARGET): $(SRC_OBJS)
	$(CXX) $^ -o $@ 

all: $(TARGET)
	

ta: 
	$(CXX) commands/noop.cpp -o bin/noop
	$(CXX) commands/number.cpp -o bin/number
	$(CXX) commands/removetag.cpp -o bin/removetag
	$(CXX) commands/removetag0.cpp -o bin/removetag0
	$(CXX) commands/manyblessings.cpp -o bin/manyblessings


test: clean all
	./$(TARGET)

clean:
	$(RM) $(TARGET) $(SRC_OBJS)