# Makefile

CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Werror -pthread -I./include
LDFLAGS := -pthread

SRCS := ./src/kvstoresystem.cpp ./src/raftnode.cpp ./src/message.cpp ./src/confdeal.cpp ./src/threadpool.cpp
OBJS := $(SRCS:.cpp=.o)

kvstoreraftsystem: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

./src/kvstoresystem.o: ./src/kvstoresystem.cpp ./include/raftnode.h 
	$(CXX) $(CXXFLAGS) -c $< -o $@

./src/raftnode.o: ./src/raftnode.cpp ./include/raftnode.h ./include/threadpool.h ./include/log.h ./include/kvstore.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

./src/message.o: ./src/message.cpp ./include/message.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

./src/confdeal.o: ./src/confdeal.cpp ./include/confdeal.h 
	$(CXX) $(CXXFLAGS) -c $< -o $@

./src/threadpool.o: ./src/threadpool.cpp ./include/threadpool.h 
	$(CXX) $(CXXFLAGS) -c $< -o $@


.PHONY: clean
clean:
	rm -f $(OBJS) kvstoreraftsystem
