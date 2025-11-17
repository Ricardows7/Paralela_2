.PHONY: all clean

CXX      := mpic++
CXXFLAGS := -std=c++11 -Wall -O3

all: shsup inputgen

shsup: shortest_superstring.cc
	$(CXX) $(CXXFLAGS) shortest_superstring.cc -o shsup

inputgen: input-generator.cc
	$(CXX) $(CXXFLAGS) input-generator.cc -o inputgen

clean:
	rm -f shsup inputgen
