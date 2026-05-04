BUILD_DIR ?= ../build/selfhost
AUREXC ?= ../build/bin/aurexc
IMPORT_FLAGS := -I src

MODULES := $(shell find src/aurex -name '*.ax' -print)
SEED_SRC := src/aurex/selfhost/bin/aurexc_seed.ax
STAGE1_SRC := src/aurex/selfhost/bin/aurexc_stage1.ax
LEXER_SMOKE_SRC := src/aurex/selfhost/smoke/lexer_smoke.ax
LEXER_RANGES_SRC := src/aurex/selfhost/smoke/lexer_ranges.ax
PARSER_SMOKE_SRC := src/aurex/selfhost/smoke/parser_smoke.ax
SEMA_ITEMS_SRC := src/aurex/selfhost/smoke/sema_items.ax
AIR_VERIFY_SRC := src/aurex/selfhost/smoke/air_verify.ax
AIR_TEXT_SRC := src/aurex/selfhost/smoke/air_text.ax
STAGE1_LANG_SRC := src/aurex/selfhost/smoke/stage1_lang.ax
STAGE1_CORE_SRC := src/aurex/selfhost/smoke/stage1_core.ax
STAGE1_IR_SRC := src/aurex/selfhost/smoke/stage1_ir.ax
STAGE1_FLOW_SRC := src/aurex/selfhost/smoke/stage1_flow.ax
STAGE1_EXPR_SRC := src/aurex/selfhost/smoke/stage1_expr.ax
STAGE1_PLACE_SRC := src/aurex/selfhost/smoke/stage1_place.ax
STAGE1_ITEMS_SRC := src/aurex/selfhost/smoke/stage1_items.ax
LEXER_DUMP_SRC := src/aurex/selfhost/tool/lexer_dump.ax
LEXER_FILE_SRC := src/aurex/selfhost/tool/lexer_file.ax
PROGRAMS := $(BUILD_DIR)/aurexc_seed $(BUILD_DIR)/lexer_smoke $(BUILD_DIR)/lexer_ranges $(BUILD_DIR)/lexer_dump $(BUILD_DIR)/lexer_file $(BUILD_DIR)/parser_smoke $(BUILD_DIR)/sema_items $(BUILD_DIR)/air_verify $(BUILD_DIR)/air_text $(BUILD_DIR)/stage1_lang $(BUILD_DIR)/stage1_core $(BUILD_DIR)/stage1_ir $(BUILD_DIR)/aurexc_stage1
STAGE_COMPILER_SOURCES := \
	src/aurex/selfhost/lexer/core.ax \
	src/aurex/selfhost/syntax/ast.ax \
	src/aurex/selfhost/parser/cursor.ax \
	src/aurex/selfhost/parser/types.ax \
	src/aurex/selfhost/parser/expr.ax \
	src/aurex/selfhost/parser/seed.ax \
	src/aurex/selfhost/sema/names.ax \
	src/aurex/selfhost/sema/calls.ax \
	src/aurex/selfhost/sema/items.ax \
	src/aurex/selfhost/sema/locals.ax \
	src/aurex/selfhost/sema/lvalues.ax \
	src/aurex/selfhost/sema/members.ax \
	src/aurex/selfhost/sema/resolve.ax \
	src/aurex/selfhost/sema/typing_types.ax \
	src/aurex/selfhost/sema/typing_lookup.ax \
	src/aurex/selfhost/sema/typing_infer.ax \
	src/aurex/selfhost/sema/annotate.ax \
	src/aurex/selfhost/sema/typing.ax \
	src/aurex/selfhost/sema/types.ax \
	src/aurex/selfhost/compiler/air/model.ax \
	src/aurex/selfhost/compiler/air/bind.ax \
	src/aurex/selfhost/compiler/air/place.ax \
	src/aurex/selfhost/compiler/air/memory.ax \
	src/aurex/selfhost/compiler/air/cfg.ax \
	src/aurex/selfhost/compiler/air/lower.ax \
	src/aurex/selfhost/compiler/air/text.ax \
	src/aurex/selfhost/compiler/air/verify.ax \
	src/aurex/selfhost/compiler/io.ax \
	src/aurex/selfhost/compiler/ir/writer.ax \
	src/aurex/selfhost/compiler/ir/names.ax \
	src/aurex/selfhost/compiler/ir/types.ax \
	src/aurex/selfhost/compiler/ir/expr.ax \
	src/aurex/selfhost/compiler/ir/cfg.ax \
	src/aurex/selfhost/compiler/ir/emit.ax \
	src/aurex/selfhost/compiler/driver.ax \
	$(STAGE1_SRC)

all: $(PROGRAMS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/aurexc_seed: $(SEED_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(SEED_SRC) -o $@

$(BUILD_DIR)/aurexc_stage1: $(STAGE1_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(STAGE1_SRC) -o $@

$(BUILD_DIR)/lexer_smoke: $(LEXER_SMOKE_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(LEXER_SMOKE_SRC) -o $@

$(BUILD_DIR)/lexer_ranges: $(LEXER_RANGES_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(LEXER_RANGES_SRC) -o $@

$(BUILD_DIR)/parser_smoke: $(PARSER_SMOKE_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(PARSER_SMOKE_SRC) -o $@

$(BUILD_DIR)/sema_items: $(SEMA_ITEMS_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(SEMA_ITEMS_SRC) -o $@

$(BUILD_DIR)/air_verify: $(AIR_VERIFY_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(AIR_VERIFY_SRC) -o $@

$(BUILD_DIR)/air_text: $(AIR_TEXT_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(AIR_TEXT_SRC) -o $@

$(BUILD_DIR)/stage1_lang: $(STAGE1_LANG_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(STAGE1_LANG_SRC) -o $@

$(BUILD_DIR)/stage1_core: $(STAGE1_CORE_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(STAGE1_CORE_SRC) -o $@

$(BUILD_DIR)/stage1_ir: $(STAGE1_IR_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(STAGE1_IR_SRC) -o $@

$(BUILD_DIR)/lexer_dump: $(LEXER_DUMP_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(LEXER_DUMP_SRC) -o $@

$(BUILD_DIR)/lexer_file: $(LEXER_FILE_SRC) $(MODULES) | $(BUILD_DIR)
	$(AUREXC) $(IMPORT_FLAGS) $(LEXER_FILE_SRC) -o $@

check: all
	$(BUILD_DIR)/aurexc_seed
	$(BUILD_DIR)/lexer_smoke
	$(BUILD_DIR)/lexer_ranges
	$(BUILD_DIR)/parser_smoke
	$(BUILD_DIR)/sema_items
	$(BUILD_DIR)/air_verify
	$(BUILD_DIR)/air_text $(BUILD_DIR)/air_text.air
	grep -q 'block_param %v' $(BUILD_DIR)/air_text.air
	grep -q 'edge fallthrough \^air0 -> \^air1 args(%v' $(BUILD_DIR)/air_text.air
	$(BUILD_DIR)/stage1_lang
	$(BUILD_DIR)/stage1_core
	$(BUILD_DIR)/stage1_ir
	$(AUREXC) $(IMPORT_FLAGS) --dump-modules $(LEXER_FILE_SRC) > $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.dump' $(BUILD_DIR)/lexer_file.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/lexer_file.modules
	$(AUREXC) $(IMPORT_FLAGS) --dump-modules $(PARSER_SMOKE_SRC) > $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.seed' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.cursor' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.expr' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.parser.types' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.lexer.core' $(BUILD_DIR)/parser_smoke.modules
	grep -q 'aurex.selfhost.syntax.ast' $(BUILD_DIR)/parser_smoke.modules
	$(AUREXC) $(IMPORT_FLAGS) --dump-modules $(STAGE1_SRC) > $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.driver' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.emit' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.model' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.bind' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.place' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.memory' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.cfg' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.lower' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.text' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.air.verify' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.expr' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.cfg' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.types' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.names' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.compiler.ir.writer' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.names' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.calls' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.items' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.locals' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.lvalues' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.members' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.resolve' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.typing_types' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.typing_lookup' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.typing_infer' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.annotate' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.typing' $(BUILD_DIR)/aurexc_stage1.modules
	grep -q 'aurex.selfhost.sema.types' $(BUILD_DIR)/aurexc_stage1.modules
	$(BUILD_DIR)/lexer_dump > $(BUILD_DIR)/lexer_dump.tokens
	$(BUILD_DIR)/lexer_file ../examples/hello.ax > $(BUILD_DIR)/lexer_file_hello.tokens
	diff -u ../tests/golden/selfhost_lexer_dump.tokens $(BUILD_DIR)/lexer_dump.tokens
	diff -u ../tests/golden/selfhost_lexer_file_hello.tokens $(BUILD_DIR)/lexer_file_hello.tokens
	../tools/compare_selfhost_lexer.sh
	$(BUILD_DIR)/aurexc_stage1 ../examples/hello.ax $(BUILD_DIR)/hello.stage1.tac
	grep -q 'aurex_tac v0' $(BUILD_DIR)/hello.stage1.tac
	grep -q 'fn puts(s: \*const u8) @puts linkage(extern_c) abi(c) -> i32' $(BUILD_DIR)/hello.stage1.tac
	grep -q 'fn main(argc: i32, argv: \*mut \*mut u8)' $(BUILD_DIR)/hello.stage1.tac
	grep -q 'c_string c"hello from Aurex M0"' $(BUILD_DIR)/hello.stage1.tac
	grep -q 'ret %t' $(BUILD_DIR)/hello.stage1.tac
	$(BUILD_DIR)/aurexc_stage1 $(SEED_SRC) $(BUILD_DIR)/aurexc_seed.stage1.tac
	grep -q 'c_string c"Aurex M0 selfhost seed"' $(BUILD_DIR)/aurexc_seed.stage1.tac
	$(BUILD_DIR)/aurexc_stage1 $(STAGE1_FLOW_SRC) $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'fn helper(limit: i32)' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'linkage(internal)' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'let one:' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'var acc:' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'assign %t' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'while %t' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'if %t' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q '\^block4:' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'air_ir v0' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'air_edges v0' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'air_cfg v0' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'inst let' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'inst assign' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'slot @slot' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'slot_addr' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'load' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'read(%v' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'store' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'effect(%v' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'lvalue @lv' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'lvalue(@lv' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'bind(param' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'bind(local' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'bind(item' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'type(' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'sema_type(type_id' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'sema_type(primitive' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge while_true \^air' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge while_false \^air0 -> \^air1' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge loop_back \^air' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge scope \^air0 -> \^air2' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge if_true \^air1 -> \^air' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge if_false \^air1 -> \^air' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge return \^air' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'stmts(3+1)' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'term branch %v' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'term jump \^air0' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge while %t' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'false \^air1' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'cfg \^air1 edge if' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge loop_back \^entry' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'edge if %t' $(BUILD_DIR)/stage1_flow.stage1.tac
	grep -q 'term ret %t' $(BUILD_DIR)/stage1_flow.stage1.tac
	$(BUILD_DIR)/aurexc_stage1 $(STAGE1_EXPR_SRC) $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'size_of <u8>' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'align_of <u8>' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'ptr_addr %t' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'ptr_from_addr <' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'cast <u8> %t' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'ptr_cast <' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'bit_cast <u32> %t' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'struct Pair' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q '.value' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'index ' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'struct_literal' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'field(' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'target_type(' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'name(' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'sema_type(integer)' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'sema_type(c_string)' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q 'sema_type(item' $(BUILD_DIR)/stage1_expr.stage1.tac
	grep -q '4]u8' $(BUILD_DIR)/stage1_expr.stage1.tac
	$(BUILD_DIR)/aurexc_stage1 $(STAGE1_PLACE_SRC) $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'assign %t' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'lvalue @lv.*binding' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'lvalue @lv.*field' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'lvalue @lv.*index' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'lvalue @lv.*deref' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'field_addr' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'index_addr' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'deref_addr' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'load' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'read(%v' $(BUILD_DIR)/stage1_place.stage1.tac
	grep -q 'lvalue(@lv' $(BUILD_DIR)/stage1_place.stage1.tac
	$(BUILD_DIR)/aurexc_stage1 $(STAGE1_ITEMS_SRC) $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'record NativeFile' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'opaque' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'enum Stage1Tag: u16' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'zero = 0' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'record Pair' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'value: i32' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'tag: aurex.selfhost.smoke.stage1_items.Stage1Tag' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'const DEFAULT_VALUE: i32 = %t' $(BUILD_DIR)/stage1_items.stage1.tac
	grep -q 'fn read_pair(pair:' $(BUILD_DIR)/stage1_items.stage1.tac
	$(BUILD_DIR)/aurexc_stage1 $(STAGE1_IR_SRC) $(BUILD_DIR)/stage1_ir.stage1.tac
	grep -q 'fn helper() @stage1_ir_helper linkage(export_c) abi(c) -> i32' $(BUILD_DIR)/stage1_ir.stage1.tac
	grep -q 'literal 40' $(BUILD_DIR)/stage1_ir.stage1.tac
	grep -q 'call %t' $(BUILD_DIR)/stage1_ir.stage1.tac
	grep -q 'add %t' $(BUILD_DIR)/stage1_ir.stage1.tac
	grep -q 'mul %t' $(BUILD_DIR)/stage1_ir.stage1.tac
	$(BUILD_DIR)/aurexc_stage1 $(STAGE_COMPILER_SOURCES) $(BUILD_DIR)/aurexc_stage1.bundle.tac
	grep -q 'aurex_tac v0' $(BUILD_DIR)/aurexc_stage1.bundle.tac
	grep -q '; selfhost_module aurex.selfhost.compiler.driver lowering(ast_pending)' $(BUILD_DIR)/aurexc_stage1.bundle.tac
	grep -q 'm0_aurex_selfhost_bin_aurexc_stage1_main' $(BUILD_DIR)/aurexc_stage1.bundle.tac

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all check clean
