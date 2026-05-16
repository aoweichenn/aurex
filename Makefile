BUILD_DIR ?= build
CMAKE ?= cmake
AUREX_GENERIC_STRESS_COUNTS ?= 100,200
AUREX_GENERIC_STRESS_MAX_RSS_MIB ?= 512
AUREX_GENERIC_STRESS_MAX_ELAPSED_MS ?= 30000
AUREX_AST_STRESS_COUNTS ?= 1000,5000
AUREX_AST_STRESS_MAX_RSS_MIB ?= 512
AUREX_AST_STRESS_MAX_ELAPSED_MS ?= 30000

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

perf-stress:
	tools/generic_stress.py
	tools/ast_stress.py

perf-stress-threshold:
	AUREX_GENERIC_STRESS_COUNTS=$(AUREX_GENERIC_STRESS_COUNTS) AUREX_GENERIC_STRESS_MAX_RSS_MIB=$(AUREX_GENERIC_STRESS_MAX_RSS_MIB) AUREX_GENERIC_STRESS_MAX_ELAPSED_MS=$(AUREX_GENERIC_STRESS_MAX_ELAPSED_MS) tools/generic_stress.py
	AUREX_AST_STRESS_COUNTS=$(AUREX_AST_STRESS_COUNTS) AUREX_AST_STRESS_MAX_RSS_MIB=$(AUREX_AST_STRESS_MAX_RSS_MIB) AUREX_AST_STRESS_MAX_ELAPSED_MS=$(AUREX_AST_STRESS_MAX_ELAPSED_MS) tools/ast_stress.py --skip-build

perf-ast-stress:
	tools/ast_stress.py

perf-compare:
	tools/frontend_compare.py

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean || true

.PHONY: all configure build test bench perf perf-stress perf-stress-threshold perf-ast-stress perf-compare clean
