# Makefile for C++ programs
# v1.4
# Made by Maxime Chretien (MixLeNain)

CXX=g++
CXXFLAGS=-Wall -O -std=c++17
LDFLAGS=
DEPFLAGS=-MMD
TARGET=main
SRCS=$(wildcard *.cpp)
OBJS=$(SRCS:.cpp=.o)
DEPS=$(OBJS:.o=.d)
LIBS=-lboost_system

.PHONY: all clean mrproper

all: $(TARGET)

exe: $(TARGET)
	./$(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LIBS)

%.o:%.cpp
	$(CXX) -o $@ -c $< $(CXXFLAGS) $(DEPFLAGS) $(LIBS)

clean:
	rm -f *.o core *.d

mrproper: clean
	rm -f $(TARGET)

-include $(DEPS)
