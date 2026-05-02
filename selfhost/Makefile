BUILD_DIR ?= ../build/selfhost
M0C ?= ../build/m0c
CC ?= cc
RUNTIME := runtime/runtime.c
IMPORT_FLAGS := -I src

SOURCES := src/m0c_seed.ax src/lexer_smoke.ax src/lexer_ranges.ax src/lexer_dump.ax src/lexer_file.ax src/parser_smoke.ax
PROGRAMS := $(BUILD_DIR)/m0c_seed $(BUILD_DIR)/lexer_smoke $(BUILD_DIR)/lexer_ranges $(BUILD_DIR)/lexer_dump $(BUILD_DIR)/lexer_file $(BUILD_DIR)/parser_smoke

all: $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.c: src/%.ax | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $< -o $@

$(BUILD_DIR)/%: $(BUILD_DIR)/%.c $(RUNTIME)
	$(CC) $< $(RUNTIME) -o $@

check: all
	$(BUILD_DIR)/m0c_seed
	$(BUILD_DIR)/lexer_smoke
	$(BUILD_DIR)/lexer_ranges
	$(BUILD_DIR)/parser_smoke
	$(M0C) $(IMPORT_FLAGS) --dump-modules src/lexer_file.ax > $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.dump' $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/lexer_file.modules
	$(M0C) $(IMPORT_FLAGS) --dump-modules src/parser_smoke.ax > $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.seed' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/parser_smoke.modules
	$(BUILD_DIR)/lexer_dump > $(BUILD_DIR)/lexer_dump.tokens
	$(BUILD_DIR)/lexer_file ../examples/hello.ax > $(BUILD_DIR)/lexer_file_hello.tokens
	diff -u ../tests/golden/selfhost_lexer_dump.tokens $(BUILD_DIR)/lexer_dump.tokens
	diff -u ../tests/golden/selfhost_lexer_file_hello.tokens $(BUILD_DIR)/lexer_file_hello.tokens
	../tools/compare_selfhost_lexer.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all check clean
