CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic

all: aurex_bootstrap

aurex_bootstrap: aurex_bootstrap.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f aurex_bootstrap hello.bootstrap.c hello.bootstrap

.PHONY: all clean
