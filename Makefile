CXX = g++
CXXFLAGS = -Wall -Wextra -pedantic -std=c++11 -g -fsanitize=address
EXEC = tcp_echo_client tcp_echo_server

all: $(EXEC)

tcp_echo_client: echo/tcp_client.cc echo/tcp_connection.h   libev/.libs/ev.o
	$(CXX) $(CXXFLAGS)  echo/tcp_client.cc libev/.libs/ev.o -I./ -o tcp_echo_client

tcp_echo_server: echo/tcp_server.cc echo/tcp_connection.h libev/.libs/ev.o
	$(CXX) $(CXXFLAGS) echo/tcp_server.cc  libev/.libs/ev.o -I./ -o tcp_echo_server

libev/.libs: libev
	git submodule update --init
	cd libev && ./configure && make

clean:
	rm -rf tcp_echo_client tcp_echo_server