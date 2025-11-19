.PHONY: all clean

CXX      := mpic++
CXXFLAGS := -std=c++11 -Wall -O3

all: shsup inputgen shsup_seq

shsup: shortest_superstring.cc
	$(CXX) $(CXXFLAGS) shortest_superstring.cc -o shsup

shsup_seq: shortest_superstring.cc
	$(CXX) $(CXXFLAGS) -fopenmp shortest_superstring.cc -o shsup_seq

inputgen: input-generator.cc
	$(CXX) $(CXXFLAGS) input-generator.cc -o inputgen

clean:
	rm -f shsup inputgen shsup_seq
