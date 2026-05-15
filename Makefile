BUILD_DIR ?= build
CMAKE ?= cmake

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR)

build: configure
	$(CMAKE) --build $(BUILD_DIR) -j

test:
	tools/run_tests.sh

bench:
	tools/bench.py

perf:
	tools/check_perf_redlines.py

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean || true

.PHONY: all configure build test bench perf clean
