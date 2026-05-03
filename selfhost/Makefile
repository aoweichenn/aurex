BUILD_DIR ?= ../build/selfhost
M0C ?= ../build/m0c
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
STAGE1_IR_SRC := src/aurex/selfhost/smoke/stage1_ir.ax
LEXER_DUMP_SRC := src/aurex/selfhost/tool/lexer_dump.ax
LEXER_FILE_SRC := src/aurex/selfhost/tool/lexer_file.ax
PROGRAMS := $(BUILD_DIR)/m0c_seed $(BUILD_DIR)/lexer_smoke $(BUILD_DIR)/lexer_ranges $(BUILD_DIR)/lexer_dump $(BUILD_DIR)/lexer_file $(BUILD_DIR)/parser_smoke $(BUILD_DIR)/stage1_lang $(BUILD_DIR)/stage1_core $(BUILD_DIR)/stage1_ir $(BUILD_DIR)/m0c_stage1
STAGE_COMPILER_SOURCES := \
	src/aurex/selfhost/lexer/core.ax \
	src/aurex/selfhost/syntax/ast.ax \
	src/aurex/selfhost/parser/cursor.ax \
	src/aurex/selfhost/parser/types.ax \
	src/aurex/selfhost/parser/expr.ax \
	src/aurex/selfhost/parser/seed.ax \
	src/aurex/selfhost/compiler/io.ax \
	src/aurex/selfhost/compiler/ir/writer.ax \
	src/aurex/selfhost/compiler/ir/names.ax \
	src/aurex/selfhost/compiler/ir/types.ax \
	src/aurex/selfhost/compiler/ir/expr.ax \
	src/aurex/selfhost/compiler/ir/emit.ax \
	src/aurex/selfhost/compiler/driver.ax \
	$(STAGE1_SRC)

all: $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/m0c_seed: $(SEED_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(SEED_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/m0c_stage1: $(STAGE1_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(STAGE1_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/lexer_smoke: $(LEXER_SMOKE_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_SMOKE_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/lexer_ranges: $(LEXER_RANGES_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_RANGES_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/parser_smoke: $(PARSER_SMOKE_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(PARSER_SMOKE_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/stage1_lang: $(STAGE1_LANG_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(STAGE1_LANG_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/stage1_core: $(STAGE1_CORE_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(STAGE1_CORE_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/stage1_ir: $(STAGE1_IR_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(STAGE1_IR_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/lexer_dump: $(LEXER_DUMP_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_DUMP_SRC) --runtime-c $(RUNTIME) -o $@

$(BUILD_DIR)/lexer_file: $(LEXER_FILE_SRC) $(MODULES) $(RUNTIME) | $(BUILD_DIR)
	$(M0C) $(IMPORT_FLAGS) $(LEXER_FILE_SRC) --runtime-c $(RUNTIME) -o $@

check: all
	$(BUILD_DIR)/m0c_seed
	$(BUILD_DIR)/lexer_smoke
	$(BUILD_DIR)/lexer_ranges
	$(BUILD_DIR)/parser_smoke
	$(BUILD_DIR)/stage1_lang
	$(BUILD_DIR)/stage1_core
	$(BUILD_DIR)/stage1_ir
	$(M0C) $(IMPORT_FLAGS) --dump-modules $(LEXER_FILE_SRC) > $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.dump' $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/lexer_file.modules
	$(M0C) $(IMPORT_FLAGS) --dump-modules $(PARSER_SMOKE_SRC) > $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.seed' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.cursor' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.expr' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.types' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.syntax.ast' $(BUILD_DIR)/parser_smoke.modules
	$(M0C) $(IMPORT_FLAGS) --dump-modules $(STAGE1_SRC) > $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.driver' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.emit' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.expr' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.types' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.names' $(BUILD_DIR)/m0c_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.writer' $(BUILD_DIR)/m0c_stage1.modules
	$(BUILD_DIR)/lexer_dump > $(BUILD_DIR)/lexer_dump.tokens
	$(BUILD_DIR)/lexer_file ../examples/hello.ax > $(BUILD_DIR)/lexer_file_hello.tokens
	diff -u ../tests/golden/selfhost_lexer_dump.tokens $(BUILD_DIR)/lexer_dump.tokens
	diff -u ../tests/golden/selfhost_lexer_file_hello.tokens $(BUILD_DIR)/lexer_file_hello.tokens
	../tools/compare_selfhost_lexer.sh
	$(BUILD_DIR)/m0c_stage1 ../examples/hello.ax $(BUILD_DIR)/hello.stage1.air
	grep -q 'aurex_ir v0' $(BUILD_DIR)/hello.stage1.air
	grep -q 'fn puts(s: \*const u8) @puts linkage(extern_c) abi(c) -> i32' $(BUILD_DIR)/hello.stage1.air
	grep -q 'fn main(argc: i32, argv: \*mut \*mut u8)' $(BUILD_DIR)/hello.stage1.air
	grep -q 'c_string c"hello from Aurex M0"' $(BUILD_DIR)/hello.stage1.air
	grep -q 'ret %e' $(BUILD_DIR)/hello.stage1.air
	$(BUILD_DIR)/m0c_stage1 $(SEED_SRC) $(BUILD_DIR)/m0c_seed.stage1.air
	grep -q 'c_string c"Aurex M0 selfhost seed"' $(BUILD_DIR)/m0c_seed.stage1.air
	$(BUILD_DIR)/m0c_stage1 $(PARSER_SMOKE_SRC) $(BUILD_DIR)/parser_smoke.stage1.air
	grep -q '; selfhost_module aurex.selfhost.smoke.parser_smoke lowering(ast_pending)' $(BUILD_DIR)/parser_smoke.stage1.air
	$(BUILD_DIR)/m0c_stage1 $(STAGE1_IR_SRC) $(BUILD_DIR)/stage1_ir.stage1.air
	grep -q 'call %e' $(BUILD_DIR)/stage1_ir.stage1.air
	grep -q 'add %e' $(BUILD_DIR)/stage1_ir.stage1.air
	grep -q 'mul %e' $(BUILD_DIR)/stage1_ir.stage1.air
	$(BUILD_DIR)/m0c_stage1 $(STAGE_COMPILER_SOURCES) $(BUILD_DIR)/m0c_stage1.bundle.air
	grep -q 'aurex_ir v0' $(BUILD_DIR)/m0c_stage1.bundle.air
	grep -q '; selfhost_module aurex.selfhost.compiler.driver lowering(ast_pending)' $(BUILD_DIR)/m0c_stage1.bundle.air
	grep -q '; selfhost_module aurex.selfhost.bin.m0c_stage1 lowering(ast_pending)' $(BUILD_DIR)/m0c_stage1.bundle.air

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all check clean
