# Makefile

CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Werror -pthread -I../include
LDFLAGS := -pthread

SRCS := ./client.cpp  ../src/message.cpp 
OBJS := $(SRCS:.cpp=.o)

client: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

./client.o: ./client.cpp ../include/message.h
	$(CXX) $(CXXFLAGS) -c $< -o $@



.PHONY: clean
clean:
	rm -f $(OBJS) client
