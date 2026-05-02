CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic

all: m0_bootstrap

m0_bootstrap: m0_bootstrap.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f m0_bootstrap hello.bootstrap.c hello.bootstrap

.PHONY: all clean
