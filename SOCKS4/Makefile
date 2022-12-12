CXX=g++
CXXFLAGS=-std=c++14 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))


SOCKS:=socks_server
SOCKS_SRC:=$(shell find socks_server_/src/ -name '*.cpp')
SOCKS_OBJS:=$(SOCKS_SRC:%.cpp=%.o) 

HW4:=hw4.cgi
CONSOLE_SRC:=$(shell find cgi_/src/ -name '*.cpp')
CONSOLE_OBJS:=$(CONSOLE_SRC:%.cpp=%.o) 


all: $(SOCKS) 


$(SOCKS):
	$(CXX) $(SOCKS_SRC) -o $@  $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

$(HW4):
	$(CXX) $(CONSOLE_SRC) -o $@  $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

test : clean all
	./socks_server 7001
	
ta: 
	$(CXX) command/delayedremovetag.cpp -o bin/delayedremovetag
	$(CXX) command/noop.cpp -o bin/noop
	$(CXX) command/number.cpp -o bin/number
	$(CXX) command/removetag0.cpp -o bin/removetag0
	$(CXX) command/removetag.cpp -o bin/removetag

clean:
	$(RM) $(SOCKS) $(SOCKS_OBJS) \
		$(HW4) $(CONSOLE_OBJS) \
		
