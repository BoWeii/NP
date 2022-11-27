CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))


HTTP:=http_server
HTTP_SRC:=$(shell find http_server_/src/ -name '*.cpp')
HTTP_OBJS:=$(HTTP_SRC:%.cpp=%.o) 

CONSOLE:=console.cgi
CONSOLE_SRC:=$(shell find cgi_/src/ -name '*.cpp')
CONSOLE_OBJS:=$(CONSOLE_SRC:%.cpp=%.o) 



part1: $(HTTP)  $(CONSOLE)

$(HTTP):
	$(CXX) $(HTTP_SRC) -o $@  $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

$(CONSOLE):
	$(CXX) $(CONSOLE_SRC) -o $@  $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

test : clean part1
	./http_server 7001
	
ta: 
	$(CXX) command/delayedremovetag.cpp -o bin/delayedremovetag
	$(CXX) command/noop.cpp -o bin/noop
	$(CXX) command/number.cpp -o bin/number
	$(CXX) command/removetag0.cpp -o bin/removetag0
	$(CXX) command/removetag.cpp -o bin/removetag

clean:
	$(RM) $(HTTP) $(HTTP_OBJS) \
		$(CONSOLE) $(CONSOLE_OBJS)