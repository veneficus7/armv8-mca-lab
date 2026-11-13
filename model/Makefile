# Builds mca-check with g++ (WSL or Linux). CLion users can open CMakeLists.txt instead.
CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic

SRC := src/parse.cpp src/model.cpp src/main.cpp

mca-check: $(SRC) src/litmus.hpp src/model.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

clean:
	rm -f mca-check

.PHONY: clean
