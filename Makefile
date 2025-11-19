.PHONY: all clean

CXX      := mpic++
CXXFLAGS := -std=c++11 -Wall -O3

all: shsup input_generator shsup_seq

shsup: shortest_superstring.cc
	$(CXX) $(CXXFLAGS) shortest_superstring.cc -o shsup

shsup_seq: shortest_superstring.cc
	$(CXX) $(CXXFLAGS) -fopenmp shortest_superstring.cc -o shsup_seq

input_generator: input-generator.cc
	$(CXX) $(CXXFLAGS) input-generator.cc -o input_generator

clean:
	rm -f shsup input_generator shsup_seq
