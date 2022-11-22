CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))


HTTP:=http_server
HTTP_SRC:=$(shell find http_server_/src/ -name '*.cpp')
HTTP_OBJS:=$(HTTP_SRC:%.cpp=%.o) 



all: $(HTTP) 

$(HTTP):
	$(CXX) $(HTTP_SRC) -o $@  $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)


test : clean all
	./http_server 7001
	
ta: 
	$(CXX) extra_files/commands/noop.cpp -o bin/noop
	$(CXX) extra_files/commands/number.cpp -o bin/number
	$(CXX) extra_files/commands/removetag.cpp -o bin/removetag
	$(CXX) extra_files/commands/removetag0.cpp -o bin/removetag0
	$(CXX) extra_files/commands/manyblessings.cpp -o bin/manyblessings


clean:
	$(RM) $(HTTP) $(HTTP_OBJS)