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
	tools/perf_report.py
	tools/frontend_compare.py

perf-compare:
	tools/frontend_compare.py

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean || true

.PHONY: all configure build test bench perf clean
