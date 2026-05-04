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

bootstrap-chain:
	tools/bootstrap_chain.sh

selfhost: build
	$(MAKE) -C selfhost check

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean || true
	$(MAKE) -C selfhost clean

.PHONY: all configure build test bench bootstrap-chain selfhost clean
