BUILD_DIR ?= ../build/selfhost
M0C ?= ../build/m0c
CC ?= cc
RUNTIME := runtime/runtime.c
IMPORT_FLAGS := -I src

MODULES := $(shell find src/aurex -name '*.ax' -print)
SEED_SRC := src/aurex/selfhost/bin/m0c_seed.ax
STAGE1_SRC := src/aurex/selfhost/bin/m0c_stage1.ax
LEXER_SMOKE_SRC := src/aurex/selfhost/smoke/lexer_smoke.ax
LEXER_RANGES_SRC := src/aurex/selfhost/smoke/lexer_ranges.ax
PARSER_SMOKE_SRC := src/aurex/selfhost/smoke/parser_smoke.ax
STAGE1_LANG_SRC := src/aurex/selfhost/smoke/stage1_lang.ax
STAGE1_CORE_SRC := src/aurex/selfhost/smoke/stage1_core.ax
LEXER_DUMP_SRC := src/aurex/selfhost/tool/lexer_dump.ax
LEXER_FILE_SRC := src/aurex/selfhost/tool/lexer_file.ax
PROGRAMS := $(BUILD_DIR)/m0c_seed $(BUILD_DIR)/lexer_smoke $(BUILD_DIR)/lexer_ranges $(BUILD_DIR)/lexer_dump $(BUILD_DIR)/lexer_file $(BUILD_DIR)/parser_smoke $(BUILD_DIR)/stage1_lang $(BUILD_DIR)/stage1_core $(BUILD_DIR)/m0c_stage1

all: $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/m0c_seed.c: $(SEED_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(SEED_SRC) -o $@

$(BUILD_DIR)/m0c_stage1.c: $(STAGE1_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(STAGE1_SRC) -o $@

$(BUILD_DIR)/lexer_smoke.c: $(LEXER_SMOKE_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_SMOKE_SRC) -o $@

$(BUILD_DIR)/lexer_ranges.c: $(LEXER_RANGES_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_RANGES_SRC) -o $@

$(BUILD_DIR)/parser_smoke.c: $(PARSER_SMOKE_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(PARSER_SMOKE_SRC) -o $@

$(BUILD_DIR)/stage1_lang.c: $(STAGE1_LANG_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(STAGE1_LANG_SRC) -o $@

$(BUILD_DIR)/stage1_core.c: $(STAGE1_CORE_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(STAGE1_CORE_SRC) -o $@

$(BUILD_DIR)/lexer_dump.c: $(LEXER_DUMP_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_DUMP_SRC) -o $@

$(BUILD_DIR)/lexer_file.c: $(LEXER_FILE_SRC) $(MODULES) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_FILE_SRC) -o $@

$(BUILD_DIR)/%: $(BUILD_DIR)/%.c $(RUNTIME)
	$(CC) $< $(RUNTIME) -o $@

check: all
	$(BUILD_DIR)/m0c_seed
	$(BUILD_DIR)/lexer_smoke
	$(BUILD_DIR)/lexer_ranges
	$(BUILD_DIR)/parser_smoke
	$(BUILD_DIR)/stage1_lang
	$(BUILD_DIR)/stage1_core
	$(M0C) $(IMPORT_FLAGS) --dump-modules $(LEXER_FILE_SRC) > $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.dump' $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/lexer_file.modules
	$(M0C) $(IMPORT_FLAGS) --dump-modules $(PARSER_SMOKE_SRC) > $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.seed' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/parser_smoke.modules
	$(M0C) $(IMPORT_FLAGS) --dump-modules $(STAGE1_SRC) > $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.driver' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.subset' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.emit_c' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.emit.bundle' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.emit.symbols' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.emit.item' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.emit.stmt' $(BUILD_DIR)/m0c_stage1.modules
	$(BUILD_DIR)/lexer_dump > $(BUILD_DIR)/lexer_dump.tokens
	$(BUILD_DIR)/lexer_file ../examples/hello.ax > $(BUILD_DIR)/lexer_file_hello.tokens
	diff -u ../tests/golden/selfhost_lexer_dump.tokens $(BUILD_DIR)/lexer_dump.tokens
	diff -u ../tests/golden/selfhost_lexer_file_hello.tokens $(BUILD_DIR)/lexer_file_hello.tokens
	../tools/compare_selfhost_lexer.sh
	$(BUILD_DIR)/m0c_stage1 ../examples/hello.ax $(BUILD_DIR)/hello.stage1.c
	$(CC) $(BUILD_DIR)/hello.stage1.c -o $(BUILD_DIR)/hello.stage1
	test "$$($(BUILD_DIR)/hello.stage1)" = "hello from Aurex M0"
	$(BUILD_DIR)/m0c_stage1 $(SEED_SRC) $(BUILD_DIR)/m0c_seed.stage1.c
	$(CC) $(BUILD_DIR)/m0c_seed.stage1.c -o $(BUILD_DIR)/m0c_seed.stage1
	test "$$($(BUILD_DIR)/m0c_seed.stage1)" = "Aurex M0 selfhost seed"
	$(BUILD_DIR)/m0c_stage1 src/aurex/selfhost/lexer/core.ax $(LEXER_SMOKE_SRC) $(BUILD_DIR)/lexer_smoke.stage1.c
	grep -q '#define validate_sequence m0_aurex_selfhost_smoke_lexer_smoke_validate_sequence' $(BUILD_DIR)/lexer_smoke.stage1.c
	grep -q 'scan_next' $(BUILD_DIR)/lexer_smoke.stage1.c
	$(CC) $(BUILD_DIR)/lexer_smoke.stage1.c -o $(BUILD_DIR)/lexer_smoke.stage1
	test "$$($(BUILD_DIR)/lexer_smoke.stage1)" = "selfhost lexer sequence ok"
	$(BUILD_DIR)/m0c_stage1 src/aurex/selfhost/lexer/core.ax $(LEXER_RANGES_SRC) $(BUILD_DIR)/lexer_ranges.stage1.c
	grep -q '#define validate_ranges m0_aurex_selfhost_smoke_lexer_ranges_validate_ranges' $(BUILD_DIR)/lexer_ranges.stage1.c
	grep -q 'scan_next' $(BUILD_DIR)/lexer_ranges.stage1.c
	$(CC) $(BUILD_DIR)/lexer_ranges.stage1.c -o $(BUILD_DIR)/lexer_ranges.stage1
	test "$$($(BUILD_DIR)/lexer_ranges.stage1)" = "selfhost lexer ranges ok"
	$(BUILD_DIR)/m0c_stage1 src/aurex/selfhost/lexer/core.ax src/aurex/selfhost/parser/seed.ax $(PARSER_SMOKE_SRC) $(BUILD_DIR)/parser_smoke.stage1.c
	grep -q '#define parse_seed_module m0_aurex_selfhost_parser_seed_parse_seed_module' $(BUILD_DIR)/parser_smoke.stage1.c
	grep -q 'scan_token' $(BUILD_DIR)/parser_smoke.stage1.c
	$(CC) $(BUILD_DIR)/parser_smoke.stage1.c -o $(BUILD_DIR)/parser_smoke.stage1
	test "$$($(BUILD_DIR)/parser_smoke.stage1)" = "selfhost parser seed ok"
	$(BUILD_DIR)/m0c_stage1 src/aurex/selfhost/lexer/core.ax src/aurex/selfhost/lexer/dump.ax $(LEXER_FILE_SRC) $(BUILD_DIR)/lexer_file.stage1.c
	grep -q '#define read_file m0_aurex_selfhost_tool_lexer_file_read_file' $(BUILD_DIR)/lexer_file.stage1.c
	grep -q 'm0_runtime_read_file' $(BUILD_DIR)/lexer_file.stage1.c
	$(CC) $(BUILD_DIR)/lexer_file.stage1.c $(RUNTIME) -o $(BUILD_DIR)/lexer_file.stage1
	$(BUILD_DIR)/lexer_file.stage1 ../examples/hello.ax > $(BUILD_DIR)/lexer_file.stage1.tokens
	diff -u ../tests/golden/selfhost_lexer_file_hello.tokens $(BUILD_DIR)/lexer_file.stage1.tokens
	$(BUILD_DIR)/m0c_stage1 $(STAGE1_LANG_SRC) $(BUILD_DIR)/stage1_lang.stage1.c
	grep -q 'aurex_m0_str' $(BUILD_DIR)/stage1_lang.stage1.c
	grep -q 'continue;' $(BUILD_DIR)/stage1_lang.stage1.c
	grep -q 'break;' $(BUILD_DIR)/stage1_lang.stage1.c
	$(CC) $(BUILD_DIR)/stage1_lang.stage1.c -o $(BUILD_DIR)/stage1_lang.stage1
	test "$$($(BUILD_DIR)/stage1_lang.stage1)" = "selfhost stage1 lang ok"
	$(BUILD_DIR)/m0c_stage1 $(STAGE1_CORE_SRC) $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'typedef struct NativeFile NativeFile;' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'typedef struct TopOpaque TopOpaque;' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'Stage1Tag_one' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'const Cell \*const_cell' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'ptr->value' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'pair_ptr->value' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'uint8_t (\*buf)\[4\]' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'alignof' $(BUILD_DIR)/stage1_core.stage1.c
	grep -q 'uintptr_t' $(BUILD_DIR)/stage1_core.stage1.c
	$(CC) $(BUILD_DIR)/stage1_core.stage1.c -o $(BUILD_DIR)/stage1_core.stage1
	test "$$($(BUILD_DIR)/stage1_core.stage1)" = "selfhost stage1 core ok"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all check clean
