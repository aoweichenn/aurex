# Aurex 编译器严谨 Bug 排查过程报告

日期：2026-05-07  
范围：前端解析、语义分析、泛型实例化、模块加载、IR lowering、IR verifier、LLVM backend、ABI 名称生成与运行时控制流。  
目标：按“系统级工业级语言”的要求，尽可能找出会导致错误代码、ABI 冲突、无效 IR、后端崩溃、诊断缺失或语义不一致的问题，并为后续修复建立回归用例基础。

## 1. 本轮排查方法

本次排查不是只跑已有单元测试，而是按编译器阶段边界逐层寻找“前一阶段放过、后一阶段崩溃或生成错误代码”的问题。

主要步骤：

1. 阅读 `src/sema/` 下的类型检查、表达式检查、声明检查、函数注册、方法查找、match 覆盖率与泛型实例化代码。
2. 阅读 `src/parse/` 下整数字面量、数组长度、限定名、结构体字面量等解析入口。
3. 阅读 `src/ir/` 下 AST lowering、match lowering、语句 lowering、全局常量 lowering 与 IR verifier。
4. 阅读 `src/backend/llvm/` 下类型映射、常量 lowering、表达式 lowering、函数和模块声明逻辑。
5. 为每个疑点写最小复现，统一放在 `/private/tmp/aurex_bughunt/`。
6. 按用户要求，所有编译或运行验证都使用 3 秒上限，避免长时间测试影响分析：

```sh
perl -e 'alarm shift @ARGV; exec @ARGV' 3 build/bin/aurexc --check <case.ax>
perl -e 'alarm shift @ARGV; exec @ARGV' 3 build/bin/aurexc --emit=ir <case.ax>
perl -e 'alarm shift @ARGV; exec @ARGV' 3 build/bin/aurexc --emit=llvm-ir <case.ax>
```

判定标准：

- `--check` 通过但 `--emit=ir`、`--emit=llvm-ir` 或可执行结果失败，通常判定为语义阶段漏诊。
- 编译全流程成功但 IR/LLVM IR 中值被截断、符号绑定错误、defer 被跳过，判定为 wrong-code 或语义破坏。
- 后端 verifier 才发现的问题，应尽量前移到 sema 或 IR verifier。
- 对系统级语言，布局、ABI、常量求值、递归类型、裸指针 mutability 与模块符号命名都按高风险处理。

本报告只整理过程和发现，没有修改生产代码。

## 2. 总体结论

已有覆盖率看起来达标，但不能覆盖这些对抗性路径。此前整体覆盖率结果为：

- `tools/check_coverage.sh`
- 行覆盖率：`95.62%`
- 函数覆盖率：`100%`
- region 覆盖率：`95.06%`

结论：覆盖率数字已经超过 95%，但当前测试集缺少大量“非法程序必须在 sema 被拒绝”和“合法边界值不能 wrong-code”的回归用例。对于编译器，覆盖率必须和负例矩阵、边界值矩阵、ABI 冲突矩阵、模块命名矩阵一起看，否则无法证明工业级可靠性。

## 3. 严重问题总览

| ID | 严重级别 | 类型 | 核心风险 |
| --- | --- | --- | --- |
| AUR-BUG-001 | Critical | wrong-code | 宽整数字面量被截断，`u64`/`i64` 初始化生成错误值 |
| AUR-BUG-002 | High | sema 漏诊 | 非 void 函数缺少返回值，`--check` 通过，IR lowering 失败 |
| AUR-BUG-003 | High | sema 漏诊 | 函数名可作为普通值使用，后端生成非法 load/return |
| AUR-BUG-004 | High | place/value 模型错误 | `&const` 通过 sema，但 lowering 只能取局部变量地址 |
| AUR-BUG-005 | High | place/value 模型错误 | rvalue 或全局 const 的结构体字段访问通过 sema，lowering 失败 |
| AUR-BUG-006 | High | coercion 缺失 | 结构体字面量字段未按声明类型强制转换，IR 聚合字段类型不匹配 |
| AUR-BUG-007 | Critical | enum 值域错误 | 非泛型 enum 判别值重复或越界不报错，match 选择错误分支 |
| AUR-BUG-008 | Critical | pattern 值域错误 | 整数 match pattern 溢出后被截断，错误匹配 |
| AUR-BUG-009 | Critical | layout/wrong-code | 数组长度字面量溢出包装成 0，`size_of` 返回错误大小 |
| AUR-BUG-010 | High | const 语义错误 | `const` 初始化允许函数调用、普通表达式等非编译期常量 |
| AUR-BUG-011 | Critical | ABI 冲突 | 显式 `@name` 重复不报错，生成重复 LLVM 符号 |
| AUR-BUG-012 | Critical | ABI 冲突 | extern c 同名 ABI 符号签名不一致，sema 不拦截 |
| AUR-BUG-013 | Critical | ABI 命名空间错误 | function/const ABI 名称冲突，LLVM 静默重命名导致绑定不可信 |
| AUR-BUG-014 | Critical | 模块 mangle 冲突 | 模块路径 mangle 后冲突，全局常量绑定到错误模块 |
| AUR-BUG-015 | High | 限定名解析错误 | import alias 下的 enum 构造器解析失败或歧义 |
| AUR-BUG-016 | High | mutability 漏诊 | `*const T` 可调用要求 `self: *mut T` 的方法 |
| AUR-BUG-017 | High | 运算符类型错误 | `~float` 被接受，LLVM 后端报非法位运算 |
| AUR-BUG-018 | High | const 依赖错误 | `const` 自引用/环引用 `--check` 通过，只在 IR verifier 失败 |
| AUR-BUG-019 | Critical | layout overflow | 合法 `u64` 数组长度的 ABI size 乘法溢出，`size_of` 返回 0 |
| AUR-BUG-020 | Critical | 泛型 enum 错误 | 泛型实例化后 enum 判别值溢出/碰撞未检查 |
| AUR-BUG-021 | Critical | 控制流错误 | `try ?` 早返回绕过 defer，破坏资源释放语义 |
| AUR-BUG-022 | High | 模块缓存错误 | symlink/规范路径缓存可让 import alias 绑定到错误模块 |
| AUR-BUG-023 | Critical | 方法 ABI 冲突 | 多模块扩展同一类型的方法生成相同 ABI 符号 |
| AUR-BUG-024 | Critical | match 覆盖率错误 | 带 guard 的 `_` 被当作真实 wildcard，导致漏报或误报 |
| AUR-BUG-025 | High | bcast 合法性错误 | 同 ABI size 的非法 bitcast 被接受，LLVM 无法 lowering 或折叠成 undef |
| AUR-BUG-026 | Critical | storage 类型错误 | `[1]void` 可作为存储，LLVM 类型映射崩溃或卡死 |
| AUR-BUG-027 | Critical | 递归类型错误 | by-value 递归 struct 被接受，size/参数/后端递归或错误 |
| AUR-BUG-028 | Critical | 递归 enum 错误 | enum payload by-value 递归被接受，payload 存储被破坏 |
| AUR-BUG-029 | High | 限定名解析错误 | 限定 struct literal 忽略 `scope_name` |
| AUR-BUG-030 | High | 泛型限定名错误 | 限定泛型类型 pattern/literal 忽略 alias scope |

## 4. 逐项问题记录

### AUR-BUG-001：宽整数字面量被截断

复现：

- `/private/tmp/aurex_bughunt/u64_literal_initializer_truncates.ax`
- `/private/tmp/aurex_bughunt/const_u64_literal_truncates.ax`
- `/private/tmp/aurex_bughunt/cast_source_literal_overflow.ax`
- `/private/tmp/aurex_bughunt/inferred_i32_literal_overflow.ax`

现象：

- `let value: u64 = 4294967296;` 语义检查通过，但 LLVM IR 中变成 `store i64 0`。
- `let value = 2147483648;` 被推断成 `i32` 后变成 `-2147483648`。
- cast 源字面量超出源类型范围时未诊断，后端按错误位宽截断。

预期：

- 字面量应该保留 arbitrary precision 或至少保留文本值直到 expected type 确定。
- 根据目标类型检查范围，不允许静默截断。
- 无 expected type 时，默认类型推断也必须拒绝超出默认类型范围的字面量。

根因位置：

- `src/sema/sema_expr.cpp`：整数字面量默认直接定为 `i32`。
- `src/ir/lower_ast_expr.cpp`：literal lowering 未充分使用 expected type。
- `src/backend/llvm/llvm_backend_value.cpp`：解析失败或溢出没有变成诊断。

建议：

- AST/CheckedExpression 中为整数字面量保存文本或 arbitrary precision 值。
- sema 在 assignment、return、call arg、struct field、array length、enum discriminant、pattern 等所有 expected-type 场景做统一范围检查。
- 后端只接收已验证的 typed integer literal，不再承担语义补救。

### AUR-BUG-002：非 void 函数缺少返回值

复现：

- `/private/tmp/aurex_bughunt/missing_return_nonvoid.ax`
- `/private/tmp/aurex_bughunt/missing_return_branch.ax`

现象：

- `--check` 通过。
- `--emit=ir` 报 `return value value id is invalid`。
- 分支中部分路径返回、部分路径落空时也未在 sema 阶段拒绝。

预期：

- 非 void 函数的所有可达路径都必须返回值。
- 这类错误应在 sema 的控制流分析阶段产生源代码诊断。

根因位置：

- `src/sema/sema_stmt.cpp`：当前只检查显式 return 语句值类型，没有完整 must-return 分析。
- `src/ir/lower_ast_stmt.cpp`：函数末尾 fallback 插入裸 return，导致非 void 返回值无效。

建议：

- 为 block/if/match/loop/break/return/defer 建立 `ControlFlowSummary`。
- 对非 void 函数要求函数体 summary 为 must-return。
- IR lowering 不应该负责修复非 void 落空路径；如果落空，应是内部错误或 verifier 错误。

### AUR-BUG-003：函数名可作为普通值使用

复现：

- `/private/tmp/aurex_bughunt/function_name_as_value.ax`

现象：

- `return value;` 中 `value` 是函数名，sema 将它当成返回类型为 `i32` 的值。
- IR 阶段出现 invalid load 或 return mismatch。

预期：

- 函数符号不能在普通值表达式位置被隐式当作返回值。
- 如果语言支持函数指针，函数名表达式也应该是函数类型或函数指针类型，而不是函数返回类型。

根因位置：

- `src/sema/function_registry.cpp`：函数 Symbol 的 type 使用了 return type。
- `src/sema/sema_expr.cpp`：name expression 读取 symbol type 后直接作为值类型。

建议：

- 区分 value symbol、function symbol、type symbol。
- 函数调用解析走 function namespace，普通表达式不能把函数名当作其返回值。

### AUR-BUG-004：`&const` 通过 sema，但 lowering 无法取地址

复现：

- `/private/tmp/aurex_bughunt/address_of_const_root.ax`

现象：

- `&VALUE` 语义检查通过。
- IR lowering 报 `store source value id is invalid`。

预期：

- 如果全局 const 没有地址语义，sema 应拒绝 `&const`。
- 如果语言允许取全局常量地址，IR lowering 应为 const 生成真实全局 storage 并返回地址。

根因位置：

- `src/sema/sema_types.cpp`：place 检查接受了 global const。
- `src/ir/lower_ast_expr.cpp`：`lower_place_address` 只支持局部变量地址。

建议：

- 明确 const 是纯值还是可寻址全局对象。
- place analysis 和 lowering 必须共享同一套 place 分类。

### AUR-BUG-005：rvalue/global const 的结构体字段访问通过 sema

复现：

- `/private/tmp/aurex_bughunt/rvalue_struct_field_access.ax`
- `/private/tmp/aurex_bughunt/const_struct_field_access.ax`

现象：

- 对临时结构体或全局 const 结构体做 `.field` 访问时，sema 只验证字段存在。
- IR lowering 需要 object address，但 rvalue/const 没有可用 place address。
- 报错包括 `field object value id is invalid`、`unknown field 'value'` 等。

预期：

- 对 rvalue 字段读取应 lowering 成 extractvalue 或临时 materialize。
- 对 const 结构体字段读取应能在 const value 上求字段，或 sema 拒绝不可寻址路径。

根因位置：

- `src/sema/sema_expr.cpp`：字段访问检查没有区分 place field 和 value field。
- `src/ir/lower_ast_expr.cpp`：字段访问 lowering 假设对象可取地址。

建议：

- 明确 field access 有两种 lowering：place projection 与 value extraction。
- sema 记录 expression category：value、place、const-value、type、function。

### AUR-BUG-006：结构体字面量字段缺少整数 coercion

复现：

- `/private/tmp/aurex_bughunt/struct_literal_integer_field_no_coerce.ax`
- `/private/tmp/aurex_bughunt/const_struct_literal_integer_field_no_coerce.ax`

现象：

- `struct S { value: u8; }` 下 `S { value: 1 }` 通过 sema。
- IR lowering 中字段 literal 仍是默认 `i32`。
- IR verifier 报 aggregate field type mismatch。

预期：

- 字段表达式应按字段 declared type 检查并 coercion。
- const struct literal 同样要应用字段类型。

根因位置：

- `src/ir/lower_ast_expr.cpp`：构造 aggregate 时未对字段值调用统一 coercion。
- 同时暴露 `AUR-BUG-001` 的 expected type 体系不足。

建议：

- sema 为每个 struct literal field 写入 expected field type。
- IR lowering 只接收已经 typed/coerced 的字段值，必要时插入显式 cast IR。

### AUR-BUG-007：非泛型 enum 判别值重复或越界未检查

复现：

- `/private/tmp/aurex_bughunt/enum_discriminant_duplicate.ax`
- `/private/tmp/aurex_bughunt/enum_discriminant_overflow_collision.ax`

现象：

- `enum Choice: u8 { a = 0, b = 0 }` 通过 sema。
- `Choice.b` 在 match 时可能先匹配 `.a`。
- 溢出值可能截断后与已有 discriminant 碰撞。

预期：

- enum case 名称不能重复。
- enum discriminant 值也不能重复。
- 显式值必须在底层整数类型范围内。

根因位置：

- `src/sema/sema_decls.cpp`：只检查 case 名称重复，没有检查判别值集合和范围。

建议：

- 对每个 enum 建立 `unordered_map<discriminant, case>`。
- 对显式/隐式 discriminant 都做 overflow 和 duplicate 检查。

### AUR-BUG-008：整数 match pattern 溢出后错误匹配

复现：

- `/private/tmp/aurex_bughunt/integer_match_pattern_overflow.ax`

现象：

- `match u8 0 { 256 => 1, _ => 2 }` 被接受。
- lowering 时 pattern `256` 按 `u8` 截断成 `0`，错误匹配第一支。

预期：

- pattern literal 应按 scrutinee 类型检查范围。
- 超出范围的 pattern 应语义报错，不能在 lowering 中截断。

根因位置：

- `src/sema/match.cpp`：pattern 检查缺少目标整数类型范围验证。
- `src/ir/lower_ast_match.cpp`：pattern 常量 lowering 使用截断后的值。

建议：

- match pattern checker 复用统一 literal range checker。
- 对 enum/integer/bool/pointer pattern 分开做 exhaustiveness 与 reachability。

### AUR-BUG-009：数组长度字面量溢出包装成 0

复现：

- `/private/tmp/aurex_bughunt/array_length_literal_overflow.ax`

现象：

- `[18446744073709551616]u8` 的长度超过 `u64`。
- parser/类型系统没有报错，最终 `size_of` 返回 0。

预期：

- 数组长度必须是可表示的非负整数。
- 超过上限必须在 parse 或 sema 阶段诊断。

根因位置：

- `src/parse/parser.cpp`：数组长度解析没有可靠处理整数溢出。
- `src/sema/sema_types.cpp`：layout size 计算没有完整溢出保护。

建议：

- parser 保留 length literal 文本；sema 统一解析为 checked `uint64_t`。
- array element size * length 使用 checked multiplication。

### AUR-BUG-010：`const` 初始化允许非编译期常量

复现：

- `/private/tmp/aurex_bughunt/const_initializer_function_call.ax`
- `/private/tmp/aurex_bughunt/const_initializer_binary_expr.ax`

现象：

- `const VALUE: i32 = f();` 通过 `--check`。
- 普通 binary expr 也可能通过 sema，但 IR verifier 报 initializer 不是 compile-time constant。

预期：

- const initializer 应限制为明确的常量表达式子集。
- 对函数调用、局部变量、非 const 全局、运行时地址等表达式应拒绝。

根因位置：

- `src/sema/sema_decls.cpp`：const initializer 只做普通表达式类型检查，没有 const-evaluable 检查。

建议：

- 建立 const-eval checker/evaluator。
- `const` 声明、enum discriminant、array length、global initializer 统一走 const evaluator。

### AUR-BUG-011：显式 ABI 符号重复不报错

复现：

- `/private/tmp/aurex_bughunt/explicit_abi_duplicate.ax`

现象：

- 两个函数使用相同 `@name("same_symbol")`。
- `--check` 通过。
- 后续 IR/LLVM 阶段出现重复符号。

预期：

- ABI symbol 是全局命名空间，重复必须在 sema 或 module verification 阶段报错。

根因位置：

- `src/sema/function_registry.cpp`：函数 registry 按源码 key 注册，没有维护 ABI 名称集合。

建议：

- 建立 module/global ABI symbol table。
- 显式 `@name` 和自动 mangle 后名称都要统一登记。

### AUR-BUG-012：extern c 同 ABI 符号签名不一致未拦截

复现：

- `/private/tmp/aurex_bughunt/extern_abi_duplicate_signature_mismatch.ax`

现象：

- 两个 extern c 声明使用同一 ABI symbol，但参数/返回类型不一致。
- `--check` 通过。
- LLVM verifier 才发现 inconsistent declaration。

预期：

- 同一 extern ABI symbol 多次声明时，签名必须完全一致。

根因位置：

- `src/sema/function_registry.cpp`：没有按 ABI symbol 合并/比较 extern declaration。

建议：

- 对 extern declaration 建立 `abi_name -> canonical_function_type` 映射。
- 不一致时报源位置，并指向首次声明。

### AUR-BUG-013：function/const ABI 命名空间互相冲突

复现：

- `/private/tmp/aurex_bughunt/function_const_abi_collision.ax`

现象：

- function 与 const 生成或指定同一 ABI symbol。
- LLVM 可能静默把其中一个改名成 `.1`。
- 这会让源语言层面的 ABI 名称与实际输出不一致。

预期：

- 所有导出/内部全局对象共享同一个 ABI 命名空间。
- 冲突必须诊断，不能依赖 LLVM 自动改名。

根因位置：

- `src/ir/verify.cpp`：当前 verifier 主要检查函数符号重复。
- `src/backend/llvm/llvm_backend_module.cpp`：LLVM 声明阶段没有强制源语言 ABI 唯一性。

建议：

- ABI symbol table 覆盖 function、extern、global const、enum/global storage、runtime entry。
- 后端生成前 assert 所有全局符号唯一。

### AUR-BUG-014：模块 mangle 冲突导致常量绑定错误

复现：

- `/private/tmp/aurex_bughunt/const_mangle_collision_root.ax`
- `/private/tmp/aurex_bughunt/const_mangle_collision/a/b_c.ax`
- `/private/tmp/aurex_bughunt/const_mangle_collision/a_b/c.ax`

现象：

- `a.b_c` 与 `a_b.c` 经过当前 mangle 规则后冲突。
- `constant_symbols_` 被覆盖，`left::VALUE` 可能实际加载右侧模块常量。

预期：

- 不同模块路径必须生成唯一 ABI/mangle 前缀。
- 冲突不能造成静默错误绑定。

根因位置：

- `src/syntax/module.cpp`：模块名 mangle 使用简单拼接/替换，不能保证可逆唯一。
- `src/ir/lower_ast.cpp`：全局常量符号表按冲突后的名称覆盖。

建议：

- 使用长度前缀编码，例如 `m1_a3_b_c` 或 hash + 原名校验。
- ABI registry 中检测 mangle collision。

### AUR-BUG-015：import alias 下的 enum 构造器解析失败或歧义

复现：

- `/private/tmp/aurex_bughunt/qualified_enum_root.ax`

现象：

- `left::Choice.some(7)` 在存在 import alias 时解析失败、歧义或落到错误的 method lookup。

预期：

- `alias::Enum.case(...)` 应解析到 alias 指向模块中的 enum constructor。
- 不应与本地同名 enum/method 混淆。

根因位置：

- `src/sema/sema_lookup.cpp`：限定名解析和 enum constructor lookup 没有完整保留 alias module identity。
- `src/sema/sema_call.cpp`：call expression 处理 enum constructor 与普通 callee 的边界不清晰。

建议：

- 将 qualified path 表示为 resolved module id + item name，而不是字符串 scope。
- enum constructor lookup 不走普通 method ambiguity 路径。

### AUR-BUG-016：`*const T` 可调用 `self: *mut T` 方法

复现：

- `/private/tmp/aurex_bughunt/method_const_pointer_to_mut_self.ax`

现象：

- 接收者是 `*const T`。
- 方法要求 `self: *mut T`。
- sema 通过，IR call argument mismatch。

预期：

- 指针 mutability 是类型系统安全边界。
- `*const T` 不能隐式传给 `*mut T`。

根因位置：

- `src/sema/sema_lookup.cpp`：method receiver matching 没有严格检查指针 mutability。
- `src/sema/sema_stmt.cpp`：调用结果检查没有兜住 receiver mismatch。

建议：

- receiver type compatibility 复用普通参数类型兼容规则。
- 对 self 参数建立专门诊断，指出 const/mut 不匹配。

### AUR-BUG-017：`~float` 被接受

复现：

- `/private/tmp/aurex_bughunt/bitwise_not_float.ax`

现象：

- `~` 位运算可作用于 float。
- LLVM 报 logical/bitwise operator only valid on integral values。

预期：

- bitwise not 只能用于整数或明确支持的 bitset 类型。

根因位置：

- `src/sema/sema_expr.cpp`：一元 `~` 没有限制 operand 类型为整数。
- `src/backend/llvm/llvm_backend_value.cpp`：后端按 bitwise op 生成 LLVM 指令后失败。

建议：

- sema 一元/二元运算符按类别建表：arithmetic、integer-only、comparison、logical、pointer comparison。

### AUR-BUG-018：`const` 自引用/环引用通过 `--check`

复现：

- `/private/tmp/aurex_bughunt/const_cycle.ax`
- `/private/tmp/aurex_bughunt/const_self.ax`

现象：

- `const A = B; const B = A;` 或 `const A = A;` 通过 `--check`。
- IR verifier 才发现 global initializer 循环或无效。

预期：

- const dependency graph 中不能存在环。

根因位置：

- `src/sema/sema_decls.cpp`：const 声明注册和 initializer 检查没有构建依赖图。
- `src/ir/verify.cpp`：后置 verifier 能发现部分问题，但太晚。

建议：

- sema 收集 const dependency graph，进行 DFS cycle detection。
- 报错应包含环上的声明链。

### AUR-BUG-019：数组 ABI size 乘法溢出

复现：

- `/private/tmp/aurex_bughunt/array_size_wrap_valid_u64.ax`

现象：

- 长度本身可表示为 `u64`，但 element size * length 超过目标地址空间。
- `size_of` 结果包装为 0 或其他错误值。

预期：

- 类型 layout size 必须 checked arithmetic。
- 超过目标平台最大 object size 应在 sema/layout 阶段报错。

根因位置：

- `src/sema/sema_types.cpp`：array ABI size 计算缺少完整溢出检查。
- `src/backend/llvm/llvm_backend_value.cpp`：`size_of` lowering 信任了错误 layout size。

建议：

- 引入 `CheckedSize`，所有 size/alignment 计算统一返回 expected/error。

### AUR-BUG-020：泛型 enum 实例化后 discriminant 溢出/碰撞

复现：

- `/private/tmp/aurex_bughunt/generic_enum_overflow.ax`

现象：

- 泛型 enum 原型通过检查。
- 实例化后底层类型或 case 值发生溢出/碰撞，但没有重新验证。

预期：

- 泛型实例化产物必须和普通声明一样完整验证。

根因位置：

- `src/sema/generic.cpp`：实例化 enum 时没有复用普通 enum declaration 的 discriminant validator。

建议：

- 抽出 enum validator，被普通声明和泛型实例化共同调用。

### AUR-BUG-021：`try ?` 早返回跳过 defer

复现：

- `/private/tmp/aurex_bughunt/try_defer_skip.ax`

现象：

- 文档中 defer 语义要求作用域退出时执行。
- `try ?` 错误路径直接 return，没有先运行当前作用域 defer。
- 复现中程序 exit 0，说明 defer 未执行。

预期：

- 所有作用域退出路径，包括 return、break、continue、try early return、panic/error return，都必须执行已注册 defer。

根因位置：

- `src/ir/lower_ast_match.cpp`：`?` lowering 直接生成 return。
- `src/ir/lower_ast_stmt.cpp`：普通 return path 有 defer emission，但 `?` path 没有复用。
- `docs/zh/api.md`：defer 语义与实现不一致。

建议：

- 所有 early-exit lowering 统一走 `emit_return_with_defers` 或类似接口。
- defer stack 必须由 lowering context 管理，不能由单一路径手工插入。

### AUR-BUG-022：import alias 可绑定到错误模块

复现：

- `/private/tmp/aurex_bughunt/symlink_import_mismatch.ax`
- `/private/tmp/aurex_bughunt/imports_symlink/`

现象：

- 两个 import path 经 symlink 或 canonical path 指向同一文件，但源文件内部 module name 与 import 期望不一致。
- module loader 可能缓存后复用错误 module identity，alias 绑定不可信。

预期：

- import path、canonical file path、declared module name 三者必须一致地校验。
- 如果同一文件被不同 module name 引入，应报错而不是复用。

根因位置：

- `src/driver/module_loader.cpp`：canonical path cache 和 expected module name 校验没有形成强约束。

建议：

- cache key 至少包含 canonical path 和 declared module id。
- 对同一 canonical path 被多个 module id 引用给出明确错误。

### AUR-BUG-023：跨模块扩展同一类型的方法 ABI 符号冲突

复现：

- `/private/tmp/aurex_bughunt/method_collision_root.ax`
- `/private/tmp/aurex_bughunt/method_collision/left.ax`
- `/private/tmp/aurex_bughunt/method_collision/right.ax`
- `/private/tmp/aurex_bughunt/method_collision/types.ax`

现象：

- 两个模块分别给同一类型扩展同名方法。
- `--check` 通过。
- `--emit=ir` 生成重复 `@m0_types_Box_read`。

预期：

- 方法 ABI symbol 必须包含定义方法所在模块，或全局检测重复。

根因位置：

- `src/sema/sema_lookup.cpp`：方法收集按 receiver type 聚合。
- `src/sema/sema_decls.cpp`：方法 ABI mangle 没有区分扩展方法定义模块。
- `src/ir/verify.cpp`：重复符号发现太晚。

建议：

- 方法 ABI 名称包含 declaring module id。
- sema 阶段维护全局 function ABI symbol registry。

### AUR-BUG-024：带 guard 的 `_` 被错误当成真实 wildcard

复现：

- `/private/tmp/aurex_bughunt/guarded_enum_wildcard_not_exhaustive.ax`
- `/private/tmp/aurex_bughunt/guarded_enum_wildcard_unreachable_false_positive.ax`

现象：

- `_ if condition => ...` 被覆盖率分析当成无条件 wildcard。
- 可能导致非穷尽 match 被误认为穷尽。
- 也可能导致后续 arm 被误报 unreachable。
- IR lowering 可出现缺少 terminator 或错误 fallthrough。

预期：

- 带 guard 的 wildcard 不是穷尽 fallback。
- 只有无 guard 的 `_` 才能覆盖剩余所有值。

根因位置：

- `src/sema/match.cpp`：coverage/reachability 逻辑没有把 guard 作为覆盖条件处理。
- `src/ir/lower_ast_match.cpp`：guarded arm lowering 与 fallback block 处理不一致。

建议：

- coverage 中将 guarded arm 视为 partial coverage。
- reachability 只在此前存在 unguarded wildcard 或全值域无 guard 覆盖时触发。

### AUR-BUG-025：`bcast` 合法性过宽

复现：

- `/private/tmp/aurex_bughunt/bcast_int_to_ptr.ax`
- `/private/tmp/aurex_bughunt/bcast_ptr_to_int.ax`
- `/private/tmp/aurex_bughunt/bcast_struct_to_i64.ax`
- `/private/tmp/aurex_bughunt/bcast_i64_to_struct.ax`
- `/private/tmp/aurex_bughunt/const_bcast_struct_to_i64_undef.ax`

现象：

- sema 只按 ABI size 判断可 bcast。
- LLVM 某些转换无法用 bitcast 表达，例如 int<->ptr 应是 inttoptr/ptrtoint。
- aggregate<->int 的 const bitcast 可能被折叠为 undef 或失败。

预期：

- `bcast` 应区分 scalar、pointer、aggregate、float、vector 等类别。
- 只允许后端能稳定表达且语义明确定义的转换。

根因位置：

- `src/sema/sema_types.cpp`：bcast 检查只看 size。
- `src/backend/llvm/llvm_backend_value.cpp`：lowering 对不同 LLVM value category 支持不完整。

建议：

- 建立 bitcast legality matrix。
- int<->ptr 如果语言允许，应使用专门 cast，不应伪装成 bcast。
- aggregate bitcast 需要通过 alloca/store/load 或禁止 const-fold 场景。

### AUR-BUG-026：`[1]void` 可作为存储类型

复现：

- `/private/tmp/aurex_bughunt/array_of_void_storage.ax`

现象：

- `void` 单独不能作为普通值类型，但 `[1]void` 可能绕过检查成为 storage。
- LLVM type lowering 对 void element array 不合法，可能报错、崩溃或卡住。

预期：

- 递归检查 storage type，数组、结构体、enum payload 中不能包含不可存储类型。

根因位置：

- `src/sema/sema_types.cpp`：storage legality 检查没有递归覆盖数组元素。
- `src/backend/llvm/llvm_backend_types.cpp`：LLVM 没有合法 `[N x void]` 类型。

建议：

- `is_storable_type` 递归检查所有组合类型。
- void、opaque value、never、function type 等只能出现在明确允许的位置。

### AUR-BUG-027：by-value 递归 struct 被接受

复现：

- `/private/tmp/aurex_bughunt/recursive_struct_by_value.ax`
- `/private/tmp/aurex_bughunt/mutual_recursive_struct_by_value.ax`
- `/private/tmp/aurex_bughunt/recursive_struct_param_by_value.ax`
- `/private/tmp/aurex_bughunt/recursive_struct_abi_size_recursion.ax`

现象：

- `struct Node { next: Node; }` 或互递归 by-value struct 通过 `--check`。
- `size_of` 可能返回 0。
- 参数按值传递时 LLVM 类型构造递归失败。
- ABI size 查询可能 sema 段崩溃。

预期：

- by-value 递归类型必须拒绝。
- 只有通过 pointer、slice、引用等 indirection 才允许递归。

根因位置：

- `src/sema/sema_decls.cpp`：结构体字段声明检查没有做递归布局图检测。
- `src/sema/sema_types.cpp`：ABI size 递归没有 cycle guard。
- `src/backend/llvm/llvm_backend_types.cpp`：LLVM type 构造对递归 value type 无法闭合。

建议：

- 构建 type layout dependency graph。
- 对 value-edge 做 cycle detection；pointer-edge 不参与无限大小判定。
- ABI size 查询必须有 visited stack，遇到非法 cycle 报诊断而不是递归。

### AUR-BUG-028：递归 enum payload 被接受并破坏 payload 存储

复现：

- `/private/tmp/aurex_bughunt/recursive_enum_payload.ax`

现象：

- enum case payload 直接包含 enum 自身。
- lowering 中把完整 `Tree` 存进按 tag/较小 payload 布局计算出来的 storage，出现类型不匹配或内存布局破坏。

预期：

- enum payload 的 by-value 递归同样必须拒绝。

根因位置：

- `src/sema/sema_decls.cpp`：enum payload 检查没有做 value recursion detection。
- `src/ir/lower_ast_match.cpp`：payload storage 假设 layout 已经合法。

建议：

- struct 与 enum 共用 layout graph validator。
- enum payload size 计算必须 checked，并有 recursion guard。

### AUR-BUG-029：限定 struct literal 忽略 `scope_name`

复现：

- `/private/tmp/aurex_bughunt/qualified_struct_literal_root.ax`
- `/private/tmp/aurex_bughunt/struct_literal_unknown_alias_accepted_root.ax`

现象：

- parser 记录了 `alias::Type { ... }` 的 scope。
- sema 解析结构体字面量时忽略 scope，只按本地或全局名字找 type。
- 未知 alias 也可能被错误接受或绑定到本地同名类型。

预期：

- `alias::Type` 必须只解析到 alias 所指模块中的 `Type`。
- 未知 alias 必须报错。

根因位置：

- `src/parse/parser_expr.cpp`：AST 已保留 scope 信息。
- `src/sema/sema_expr.cpp`：struct literal resolution 忽略 scope。

建议：

- struct literal lowering 前的 sema type resolution 必须消费 qualified path。
- 禁止“scope 解析失败后退回本地同名”的行为。

### AUR-BUG-030：限定泛型类型 pattern/literal 忽略 alias scope

复现：

- `/private/tmp/aurex_bughunt/qualified_generic_inference_root.ax`
- `/private/tmp/aurex_bughunt/qualified_generic_inference/a.ax`
- `/private/tmp/aurex_bughunt/qualified_generic_inference/b.ax`
- `/private/tmp/aurex_bughunt/qualified_generic_inference/util.ax`

现象：

- `alias::Generic<T>` 在推断、pattern、literal 路径中没有稳定保留模块 alias。
- 可能解析到本地同名泛型或另一个模块的同名泛型。

预期：

- 泛型实例化 key 必须包含 resolved module id。
- AST 的 qualified path 不应被字符串化后丢失语义。

根因位置：

- `src/sema/generic.cpp`：泛型推断/实例化路径没有完整使用 resolved module scope。
- `src/sema/sema_expr.cpp`：literal/type lookup 忽略部分 scope 信息。

建议：

- 所有类型引用使用 `QualifiedTypeId` 或 `ResolvedItemId`。
- generic cache key 包含 module id、decl id、type args。

## 5. 根因聚类

### 5.1 字面量和常量体系不够强

关联问题：

- AUR-BUG-001
- AUR-BUG-006
- AUR-BUG-008
- AUR-BUG-009
- AUR-BUG-010
- AUR-BUG-018
- AUR-BUG-019
- AUR-BUG-020

根因总结：

- 字面量过早落成固定机器类型。
- expected type 没有贯穿 sema 到 lowering。
- const expression 与普通 expression 没有严格分层。
- layout size 和 discriminant 没有统一 checked arithmetic。

### 5.2 名称解析、模块 identity、ABI symbol 没有统一模型

关联问题：

- AUR-BUG-011
- AUR-BUG-012
- AUR-BUG-013
- AUR-BUG-014
- AUR-BUG-015
- AUR-BUG-022
- AUR-BUG-023
- AUR-BUG-029
- AUR-BUG-030

根因总结：

- 源语言名称、module path、import alias、declared module name、ABI symbol、LLVM global name 是不同层概念，但当前多处用字符串拼接/替换承载。
- mangle 规则不可逆且可能碰撞。
- ABI registry 没有覆盖所有全局对象。

### 5.3 place/value/function/type namespace 混在一起

关联问题：

- AUR-BUG-003
- AUR-BUG-004
- AUR-BUG-005
- AUR-BUG-016

根因总结：

- 表达式没有稳定记录 category。
- 函数符号可被当作值符号。
- sema 接受的 place 与 lowering 支持的 place 不一致。
- receiver mutability 没有复用参数类型兼容规则。

### 5.4 控制流和资源释放语义缺少统一出口

关联问题：

- AUR-BUG-002
- AUR-BUG-021
- AUR-BUG-024

根因总结：

- 非 void must-return 没有完整控制流分析。
- return 与 try early-return 各自 lowering，defer 执行路径不统一。
- match guard 对覆盖率和可达性分析的影响没有建模。

### 5.5 类型布局和后端合法性检查后置

关联问题：

- AUR-BUG-017
- AUR-BUG-025
- AUR-BUG-026
- AUR-BUG-027
- AUR-BUG-028

根因总结：

- sema 允许了一些 LLVM 无法表达或源语言不应允许的类型/操作。
- ABI size、storage legality、递归 layout 没有形成前端硬约束。
- backend 在替 sema 承担诊断职责，导致错误位置晚且不稳定。

## 6. 建议修复优先级

### P0：先修 wrong-code 与 ABI 冲突

1. 修复整数字面量 expected-type/range/coercion 体系。
2. 修复 enum discriminant、match integer pattern、array length、array layout size 的 checked arithmetic。
3. 建立全局 ABI symbol registry，覆盖 function、extern、const、method、自动 mangle 名称。
4. 修复 module mangle，保证 collision-free。
5. 修复 `try ?` 跳过 defer。

理由：这些问题会生成“看起来成功但行为错误”的程序，是系统级语言最危险的 bug。

### P1：补齐 sema 阶段硬错误

1. 非 void must-return 控制流分析。
2. const initializer const-evaluable 检查和 const dependency cycle 检查。
3. 函数名表达式 namespace 分离。
4. `~float`、bcast 合法性矩阵、receiver mutability 检查。
5. struct literal 字段 coercion。

理由：这些问题当前会在 IR/LLVM 才暴露，诊断晚且难懂。

### P2：统一 place/value 和限定名解析

1. 表达式 category：value/place/type/function/const-value。
2. 字段访问支持 rvalue extraction 或 sema 明确拒绝。
3. struct literal、enum constructor、generic inference 全部使用 resolved module id。
4. import alias 和 canonical path 校验。

理由：这些问题容易在项目规模变大、多模块同名类型增多后变成难查的绑定错误。

### P3：类型 layout validator

1. 拒绝 by-value 递归 struct/enum。
2. 递归检查 storage legality，禁止 `[N]void` 等非法组合。
3. 所有 ABI size/alignment 查询带 cycle guard 和 overflow guard。

理由：这是后端稳定性的底座，修复后能减少崩溃、卡死和 LLVM verifier 错误。

## 7. 建议新增回归测试矩阵

每个已确认问题都应至少有一个负例或 wrong-code 回归测试。建议分目录：

- `tests/negative/literals/`
- `tests/negative/const_eval/`
- `tests/negative/abi/`
- `tests/negative/modules/`
- `tests/negative/match/`
- `tests/negative/layout/`
- `tests/negative/place_value/`
- `tests/codegen/wrong_code/`

关键断言方式：

- 对应 sema 应拒绝的用例，测试 `--check` 非 0，并匹配核心诊断文本。
- 对应 IR verifier 的用例，修复后应在 sema 拒绝，不应进入后端。
- 对 wrong-code 用例，修复前可记录 LLVM IR 中错误值；修复后运行结果必须匹配源语言语义。
- ABI collision 测试应覆盖 function/function、extern/extern、function/const、method/method、module mangle collision。

建议把 `/private/tmp/aurex_bughunt/` 中已确认的 `.ax` 文件迁移到正式测试目录，并在测试名中带上 bug ID。

## 8. 复现用例索引

### 8.1 已确认问题用例

```text
/private/tmp/aurex_bughunt/u64_literal_initializer_truncates.ax
/private/tmp/aurex_bughunt/const_u64_literal_truncates.ax
/private/tmp/aurex_bughunt/cast_source_literal_overflow.ax
/private/tmp/aurex_bughunt/inferred_i32_literal_overflow.ax
/private/tmp/aurex_bughunt/missing_return_nonvoid.ax
/private/tmp/aurex_bughunt/missing_return_branch.ax
/private/tmp/aurex_bughunt/function_name_as_value.ax
/private/tmp/aurex_bughunt/address_of_const_root.ax
/private/tmp/aurex_bughunt/rvalue_struct_field_access.ax
/private/tmp/aurex_bughunt/const_struct_field_access.ax
/private/tmp/aurex_bughunt/struct_literal_integer_field_no_coerce.ax
/private/tmp/aurex_bughunt/const_struct_literal_integer_field_no_coerce.ax
/private/tmp/aurex_bughunt/enum_discriminant_duplicate.ax
/private/tmp/aurex_bughunt/enum_discriminant_overflow_collision.ax
/private/tmp/aurex_bughunt/integer_match_pattern_overflow.ax
/private/tmp/aurex_bughunt/array_length_literal_overflow.ax
/private/tmp/aurex_bughunt/const_initializer_function_call.ax
/private/tmp/aurex_bughunt/const_initializer_binary_expr.ax
/private/tmp/aurex_bughunt/explicit_abi_duplicate.ax
/private/tmp/aurex_bughunt/extern_abi_duplicate_signature_mismatch.ax
/private/tmp/aurex_bughunt/function_const_abi_collision.ax
/private/tmp/aurex_bughunt/const_mangle_collision_root.ax
/private/tmp/aurex_bughunt/qualified_enum_root.ax
/private/tmp/aurex_bughunt/method_const_pointer_to_mut_self.ax
/private/tmp/aurex_bughunt/bitwise_not_float.ax
/private/tmp/aurex_bughunt/const_cycle.ax
/private/tmp/aurex_bughunt/const_self.ax
/private/tmp/aurex_bughunt/array_size_wrap_valid_u64.ax
/private/tmp/aurex_bughunt/generic_enum_overflow.ax
/private/tmp/aurex_bughunt/module_mangle_collision.ax
/private/tmp/aurex_bughunt/try_defer_skip.ax
/private/tmp/aurex_bughunt/symlink_import_mismatch.ax
/private/tmp/aurex_bughunt/method_collision_root.ax
/private/tmp/aurex_bughunt/guarded_enum_wildcard_not_exhaustive.ax
/private/tmp/aurex_bughunt/guarded_enum_wildcard_unreachable_false_positive.ax
/private/tmp/aurex_bughunt/bcast_int_to_ptr.ax
/private/tmp/aurex_bughunt/bcast_ptr_to_int.ax
/private/tmp/aurex_bughunt/bcast_struct_to_i64.ax
/private/tmp/aurex_bughunt/bcast_i64_to_struct.ax
/private/tmp/aurex_bughunt/const_bcast_struct_to_i64_undef.ax
/private/tmp/aurex_bughunt/array_of_void_storage.ax
/private/tmp/aurex_bughunt/recursive_struct_by_value.ax
/private/tmp/aurex_bughunt/mutual_recursive_struct_by_value.ax
/private/tmp/aurex_bughunt/recursive_struct_param_by_value.ax
/private/tmp/aurex_bughunt/recursive_struct_abi_size_recursion.ax
/private/tmp/aurex_bughunt/recursive_enum_payload.ax
/private/tmp/aurex_bughunt/qualified_struct_literal_root.ax
/private/tmp/aurex_bughunt/struct_literal_unknown_alias_accepted_root.ax
/private/tmp/aurex_bughunt/qualified_generic_inference_root.ax
```

### 8.2 配套多模块用例文件

```text
/private/tmp/aurex_bughunt/qualified_enum/a.ax
/private/tmp/aurex_bughunt/qualified_enum/b.ax
/private/tmp/aurex_bughunt/qualified_enum_case/a.ax
/private/tmp/aurex_bughunt/qualified_enum_case/b.ax
/private/tmp/aurex_bughunt/qualified_enum_case_root.ax
/private/tmp/aurex_bughunt/imports/a/b_c.ax
/private/tmp/aurex_bughunt/imports/a_b/c.ax
/private/tmp/aurex_bughunt/const_mangle_collision/a/b_c.ax
/private/tmp/aurex_bughunt/const_mangle_collision/a_b/c.ax
/private/tmp/aurex_bughunt/method_collision/left.ax
/private/tmp/aurex_bughunt/method_collision/right.ax
/private/tmp/aurex_bughunt/method_collision/types.ax
/private/tmp/aurex_bughunt/qualified_struct_literal/a.ax
/private/tmp/aurex_bughunt/qualified_struct_literal/b.ax
/private/tmp/aurex_bughunt/struct_literal_unknown_alias_accepted/a.ax
/private/tmp/aurex_bughunt/qualified_generic_inference/a.ax
/private/tmp/aurex_bughunt/qualified_generic_inference/b.ax
/private/tmp/aurex_bughunt/qualified_generic_inference/util.ax
```

### 8.3 探索过但未作为本报告确认 bug 的用例

这些文件在排查中用于缩小范围，其中部分已经被 parser/sema 正确拒绝，或需要进一步明确语言规范后才能判定：

```text
/private/tmp/aurex_bughunt/bool_cast_sign_extends.ax
/private/tmp/aurex_bughunt/bool_cast_variable_sign_extends.ax
/private/tmp/aurex_bughunt/const_bool_cast.ax
/private/tmp/aurex_bughunt/if_expr_return_phi.ax
/private/tmp/aurex_bughunt/match_expr_return_phi.ax
/private/tmp/aurex_bughunt/short_circuit_return_phi.ax
/private/tmp/aurex_bughunt/if_expr_null_expected_pointer.ax
/private/tmp/aurex_bughunt/array_of_opaque_storage.ax
/private/tmp/aurex_bughunt/empty_abi_name.ax
/private/tmp/aurex_bughunt/variadic_struct_arg.ax
/private/tmp/aurex_bughunt/value_symbol_namespace_root.ax
```

说明：

- `bool_cast_*` 当前生成 `true -> 1`，未确认存在 sign-extension 错误。
- `if_expr_return_phi.ax`、`match_expr_return_phi.ax`、`short_circuit_return_phi.ax` 当前被 parser 拒绝，不作为本轮 bug。
- `if_expr_null_expected_pointer.ax` 当前被 sema 拒绝；是否应利用 expected pointer type 推断 `null`，属于语言设计问题。
- `array_of_opaque_storage.ax` 当前被 sema 正确拒绝。
- `empty_abi_name.ax` 和 `variadic_struct_arg.ax` 需要先明确语言规范：是否允许空 ABI 名、是否限制 variadic aggregate 参数。
- `value_symbol_namespace_root.ax` 的命名空间问题需要配合 module search path 重新固定复现。

## 9. 后续实施建议

建议先不要零散修补单个后端错误，而是按以下顺序改：

1. 建立统一 `Diagnostic` 流程：所有 sema validator 能带源位置报错。
2. 建立 `CheckedIntegerLiteral` / `ConstValue` / `CheckedSize` 三个基础设施。
3. 建立 `ResolvedItemId` / `ModuleId` / collision-free mangle。
4. 建立全局 ABI registry。
5. 建立 expression category，拆清 value/place/type/function。
6. 建立 layout graph validator。
7. 建立 control-flow summary 与 defer-aware early-exit lowering。
8. 将本报告 8.1 中的复现用例迁入测试，并为每个 bug ID 加测试。

## 10. 继续修复记录

本节记录 2026-05-07 后续修复过程。用户将“单个定向用例”的性能判断从 3 秒收紧到百毫秒级，因此本轮验证以单样例 `--check`、`--dump-ir`、`--dump-llvm-ir` 和必要的单文件编译运行为主；增量 C++ 构建单独计时，不把工具链冷启动和链接开销混入 sema 性能判断。

### 10.1 常量一元表达式被 sema/IR/backend 不一致处理

问题：

- `const X: i32 = -1;`
- `const FLAG: bool = !false;`
- `const MASK: i32 = ~0;`

语义层此前把一元表达式视为非编译期常量；放开后，IR verifier 和 LLVM 常量 lowering 又缺少 `ValueKind::unary` 支持，会报 `constant initializer is not compile-time constant` 或退化为 `undef` 风险。

修复：

- `src/sema/sema_decls.cpp`：`is_const_evaluable_expr` 允许 `!`、一元 `-`、`~` 的常量操作数。
- `src/ir/verify.cpp`：常量 verifier 递归校验 `ValueKind::unary`，并明确拒绝 address/deref 这类运行期一元操作。
- `src/backend/llvm/llvm_backend_value.cpp`：新增 `emit_constant_unary`，对 bool/integer not、integer negate、float negate 生成 LLVM constant。
- `src/backend/llvm/llvm_backend_internal.hpp`：声明新增 helper。

回归：

- `tests/samples/positive/types/const_unary.ax`

验证：

- `build/bin/aurexc --check tests/samples/positive/types/const_unary.ax`
- `build/bin/aurexc --dump-llvm-ir tests/samples/positive/types/const_unary.ax`
- 单文件编译并运行，退出码为 0。

### 10.2 相同签名的 extern C 重复声明被误判为 ABI 冲突

问题：

两个 Aurex 声明指向同一个 C ABI 符号且签名完全一致时，当前 ABI registry 仍然报 `duplicate ABI symbol`。这和后端 `extern_functions_` 复用同名 LLVM declaration 的设计不一致，也会阻止多模块重复声明同一个 C API。

修复：

- `src/sema/sema_decls.cpp`：`validate_abi_symbols` 对两个 `extern c` 函数同名且签名一致的情况直接允许；仍然拒绝签名不一致、extern 与非 extern 定义冲突、函数与常量 ABI 冲突。

回归：

- `tests/samples/positive/functions/extern_c_redeclare_same_signature.ax`
- 继续保留并验证 `tests/samples/negative/functions/extern_abi_signature_mismatch.ax`

验证：

- 正例 `--check` 通过。
- `--dump-llvm-ir` 中只生成一个 `declare i32 @aurex_test_same_extern_symbol(i32)`。
- 负例仍在 sema 阶段报 `extern C ABI symbol redeclared with incompatible signature`。

### 10.3 `paddr` 宽地址字面量被错误按 i32 检查

问题：

`paddr(*const T, 4294967296)` 在 64 位目标上应按地址宽度处理，但地址实参原来没有 expected type，整数字面量被默认按 `i32` 检查，导致误报溢出。

修复：

- `src/sema/sema_expr.cpp`：分析 `paddr` 的地址实参时传入 `usize` expected type。

回归：

- `tests/samples/positive/pointers/paddr_wide_literal.ax`
- 继续验证 `tests/samples/negative/pointers/paddr_non_pointer.ax`
- 继续验证 `tests/samples/negative/expressions/expression_diagnostics_matrix.ax`

验证：

- 正例 `--check` 和 `--dump-llvm-ir` 通过。
- LLVM IR 中宽字面量保持为 `inttoptr (i64 4294967296 to ptr)`。
- 非指针目标和非整数地址仍然在 sema 阶段报错。

### 10.4 二元/嵌套一元表达式没有传播 expected integer type

问题：

编译器已经在赋值、返回、调用实参、结构体字段等场景支持整数字面量 expected type，但二元表达式内部没有传播，导致以下合法边界表达式被错误地按默认 `i32` 检查：

- `let wide: i64 = 2147483648 + 1;`
- `wide != 2147483649`
- `2147483649 != wide`
- `let nested: i64 = -(2147483648 + 1);`

修复：

- `src/sema/sema_expr.cpp`：
  - 对结果类型等于操作数类型的二元运算，将外层 expected numeric type 传入操作数。
  - 当左侧是上下文敏感整数字面量、右侧有明确类型时，先分析右侧，再用右侧类型回填左侧。
  - 对非字面量一元 `-`/`~` 继续向操作数传递合法 expected type。

回归：

- `tests/samples/positive/expressions/expected_integer_binary.ax`
- 继续验证 `tests/samples/negative/types/inferred_i32_literal_overflow.ax`
- 继续验证 `tests/samples/negative/types/cast_source_literal_overflow.ax`
- 继续验证 signed min/unsigned negative 负例。

验证：

- 正例 `--check`、`--dump-llvm-ir`、单文件运行均通过。
- LLVM IR 保留 `i64 2147483649` 和 `i64 -2147483649`。
- 无上下文的宽整数字面量和显式 cast 源字面量仍按默认 `i32` 规则报溢出。

### 10.5 `null` 在指针 expected type 内部表达式中丢失类型

问题：

直接赋值或返回 `null` 时，`can_assign` 能特判指针目标；但 `if`、block、match 等表达式内部会先把 `null` 记录成 invalid type，导致合法指针表达式被误拒绝，或在 match 中把 `null` 错误地降成非指针常量。

示例风险：

- `return if flag { null } else { null };`
- `let inferred = if false { null } else { choose(false) };`
- `let inferred = match false { true => null, false => choose(false) };`
- `return match true { true => null, false => 1 };` 旧逻辑可能把整个 match 定成 `i32`，后端再处理 invalid/null 组合。

修复：

- `src/sema/sema_expr.cpp`：
  - `null_literal` 在 expected pointer type 下记录成该指针类型。
  - `if` 表达式在无外层 expected 且 then 分支是 null 结果时，先分析 else 分支，再用 else 类型回填 then 分支。
  - 支持 block expression 最终结果为 null 的分支。
- `src/sema/match.cpp`：
  - 记录待定 null arm。
  - 后续结果类型为指针时允许回填；结果类型为非指针时立即报 `match expression arms must have the same type`。

回归：

- `tests/samples/positive/pointers/null_if_expr.ax`
- `tests/samples/positive/pointers/null_match_expr.ax`
- `tests/samples/negative/pattern_matching/match_null_with_non_pointer_arm.ax`
- 继续验证 `tests/samples/negative/inference/local_inference_null.ax`

验证：

- 指针正例 `--check`、`--dump-ir`、单文件运行均通过。
- IR 中 `if`/`match` 的 phi 类型为 `*const i32`。
- 无上下文 `let value = null;` 仍无法推断类型。
- `match` 的 null/non-pointer arm 组合在 sema 阶段报错，不再进入 lowering。

### 10.6 `bcast` 允许 bool 与 u8 互转，LLVM i1/i8 语义不匹配

问题：

`bool` 的 LLVM 表示是 `i1`，但 ABI size 计算按 1 byte。旧的 `bcast` 合法性只看 ABI size，因此会放过：

- `bcast(bool, cast(u8, 2))`
- `bcast(u8, true)`

这两类转换在 LLVM 层不是合法的同宽 bitcast，容易退化成非法 IR、poison 或错误折叠。

修复：

- `src/sema/sema_types.cpp`：`is_builtin_scalar_bcast_type` 排除 `bool`。
- 后续 M2 清理已移除语言级 copyability；当前同类型 `bitcast[T](value: T)` 只允许数值标量和指针这类可位级重解释类型。

回归：

- `tests/samples/negative/types/bcast_bool_from_u8.ax`
- `tests/samples/negative/types/bcast_u8_from_bool.ax`
- 继续验证 `tests/samples/positive/core/primitive_matrix.ax`

验证：

- 两个负例均在 sema 阶段报 `invalid explicit conversion`。
- primitive matrix 的 `u32 <-> f32`、`u64 <-> f64` 合法 bitcast 仍通过。

### 10.7 非字面量无符号整数取负被错误接受

问题：

`let x: u32 = -1;` 已经会因负字面量不适配 unsigned 被拒绝，但以下形式会绕过字面量特判：

```m0
let one: u32 = 1;
let value: u32 = -one;
```

旧 sema 只要求一元 `-` 的操作数是 integer 或 float，因此会接受 unsigned 变量取负，后端再生成 wrapping negate。

修复：

- `src/sema/sema_expr.cpp`：新增 signed integer 判定；一元 numeric negate 只允许 signed integer 或 float。

回归：

- `tests/samples/negative/types/negative_unsigned_expression.ax`
- 继续验证 `tests/samples/negative/types/negative_unsigned_literal.ax`
- 继续验证 `tests/samples/positive/types/signed_min_literals.ax`

验证：

- 非字面量 unsigned 取负报 `numeric unary operator requires signed integer or float operand`。
- signed min 字面量仍可通过。

### 10.8 shift 字面量右操作数缺少 poison 防护

问题：

LLVM shift 对负数位移和 `amount >= bit_width` 是 poison。旧 sema 只检查类型，因此会放过：

- `1 << 32`，当左操作数为 `i32`
- `1 << -1`

修复：

- `src/sema/sema_expr.cpp`：
  - 对 `shl`/`shr` 的字面量右操作数检查 bit width。
  - 负整数字面量右操作数报 `shift amount cannot be negative`。
  - 非负整数字面量超过或等于左操作数位宽时报 `shift amount is out of range`。
  - `-0` 不再被误判为负 shift amount。

回归：

- `tests/samples/negative/expressions/shift_amount_overflow.ax`
- `tests/samples/negative/expressions/shift_amount_negative.ax`
- `tests/samples/positive/expressions/shift_amount_negative_zero.ax`
- 继续验证 `tests/samples/positive/expressions/operator_matrix.ax`

验证：

- 两个负例均在 sema 阶段失败。
- `1 << -0` 正例 `--check`、单文件编译、运行均通过。

### 10.9 整数 `/`、`%` 的静态 poison 场景未被拒绝

问题：

LLVM integer `sdiv/udiv/srem/urem` 对除 0/模 0 是 poison；signed `INT_MIN / -1` 和 `INT_MIN % -1` 也是 poison。旧 sema 会放过这些确定错误的字面量表达式：

- `1 / 0`
- `1 % 0`
- `1 / -0`
- `-2147483648 / -1`
- `-2147483648 % -1`

修复：

- `src/sema/sema_expr.cpp`：
  - 复用整数字面量/一元负号字面量识别。
  - 对 integer `/`、`%` 的 RHS 字面量 0 报错。
  - 对 signed min literal 与 `-1` 的 `/`、`%` 报 signed overflow。

回归：

- `tests/samples/negative/expressions/integer_division_by_zero.ax`
- `tests/samples/negative/expressions/integer_modulo_by_zero.ax`
- `tests/samples/negative/expressions/integer_division_by_negative_zero.ax`
- `tests/samples/negative/expressions/signed_division_overflow.ax`
- `tests/samples/negative/expressions/signed_modulo_overflow.ax`

验证：

- 所有负例均在 sema 阶段失败，错误分别指向 zero RHS 或整个 overflowing binary expression。
- `tests/samples/positive/expressions/operator_matrix.ax` 仍通过。
- `tests/samples/positive/types/signed_min_literals.ax` 仍通过。

### 10.10 `cast(bool, nonzero)` 被后端错误降成低位截断

问题：

语义层允许 numeric cast 到 `bool`。旧 LLVM lowering 对 integer-to-bool 使用 int cast，导致：

- `cast(bool, 2)` 截断成 `i1 0`，运行结果为 false。
- float-to-bool 使用 `fptoui/fptosi` 到 `i1`，对 `2.0` 等值可能进入 LLVM out-of-range poison。

修复：

- `src/backend/llvm/llvm_backend_value.cpp`：
  - 运行时 numeric cast 到 bool 改为和 0 比较。
  - integer 使用 `icmp ne x, 0`。
  - float 使用 `fcmp une x, 0.0`，NaN 按非零处理。
  - 常量 numeric cast 到 bool 使用 LLVM constant fold compare；折叠失败时返回目标类型 `undef`，避免空指针崩溃。

回归：

- `tests/samples/positive/types/cast_bool_nonzero.ax`

验证：

- 修复前样例编译运行返回 1。
- 修复后 `--check`、`--dump-llvm-ir`、单文件运行均通过。
- LLVM IR 中 `const bool = cast(bool, 2)` 降成 `i1 true`，`cast(bool, 0)` 降成 `i1 false`。

### 10.11 浮点 `!=` 对 NaN 使用了错误 predicate

问题：

旧后端把 float not-equal 降成 ordered-not-equal：`fcmp one`。这会导致 `NaN != NaN` 返回 false，不符合 IEEE 风格的非等比较语义。

修复：

- `src/backend/llvm/llvm_backend_value.cpp`：float `!=` 从 `CreateFCmpONE` 改为 `CreateFCmpUNE`。
- float `==` 保持 ordered-equal，`NaN == NaN` 仍为 false。

回归：

- `tests/samples/positive/expressions/float_nan_comparison.ax`

验证：

- 修复前样例运行返回 2。
- 修复后 `--check`、`--dump-llvm-ir`、单文件运行均通过。
- LLVM IR 中 `!=` 为 `fcmp une double`。

### 10.12 补充全局 `const str` / `const c"..."` 后端覆盖

结论：

- `const TEXT: str = "hi";`
- `const C_TEXT: *const u8 = c"hi";`

当前后端能生成稳定 LLVM 全局常量：

- `str` 降成 `{ ptr, i64 }` aggregate constant。
- C string 降成带 `\0` 的 private global，并把 const pointer 指向该 global。

回归：

- `tests/samples/positive/types/const_string.ax`

验证：

- `--check`、`--dump-llvm-ir`、单文件编译运行均通过。
- 注意：`str` 当前不是普通结构体类型，不能直接写 `text.len` / `text.data`。本样例只覆盖全局常量 lowering，不改变 `str` 字段访问设计。

后续继续找 bug 时，优先级仍应放在“sema 能静态证明但旧逻辑放过、最终会进入 LLVM poison/undef/非法 IR”的路径上；这类问题单测小、定位快、收益高。

### 10.13 IR verifier 缺少关键结构不变量

问题：

IR verifier 过去只检查了一部分 value id / type 存在性，缺少几类会直接污染后端的结构不变量：

- `store` 的 source type 可以和 target pointee type 不一致。
- `unary` / `binary` 的 operand/result 约束不完整。
- `cast` 的 result type 可以和 `target_type` 不一致。
- `phi` incoming predecessor 可以不是当前 block 的真实 CFG predecessor。
- `phi` 可以缺少真实 predecessor 的 incoming，或重复同一个 predecessor。

这类问题如果在 verifier 漏掉，LLVM backend 可能在更晚阶段崩溃、生成非法 IR，或把错误表现成难定位的后端问题。

修复：

- `src/ir/verify.cpp`：
  - `store` 校验 result 必须是 `void`，source type 必须等于 target pointee type。
  - 新增 unary verifier：`!` 只允许 bool->bool，数值取负保持 numeric type，`~` 保持 integer type。
  - 新增 binary verifier：校验两侧 operand type、numeric/integer/logical/comparison/equality 的 result type。
  - `cast` 校验 result type 等于 `target_type`。
  - `phi` 校验 predecessor set 和 CFG edge 精确匹配，拒绝重复 incoming。

回归：

- `tests/gtest/ir/ir_verifier_structural_tests.cpp`
- `tests/gtest/ir/ir_verifier_edge_tests.cpp`

验证：

- `build/bin/aurex_tests --gtest_filter=CoreUnit.IrVerifierReportsRepresentativeStructuralErrors`
- `build/bin/aurex_tests --gtest_filter=CoreUnit.IrVerifierReportsAdditionalEdgeCaseErrors`
- `build/bin/aurex_tests --gtest_filter=CoreUnit.IrVerifierReportsRuntimeShapeErrors`
- 多个正例 `--dump-ir` 通过，说明新 verifier 没有拒绝当前正常 lowering。

### 10.14 指向数组的指针下标语义错误

问题：

`*mut [2]i32` 直接写 `values[0][0]` 时，旧 sema 把第一次下标当成普通 pointer indexing：

```text
%3 : *mut [2]i32 = load %0
%5 : *mut [2]i32 = index_addr %3[%4]
%7 : *mut i32 = index_addr %5[%6]
```

这表示 `values + 0` 得到“另一个数组地址”，再对数组元素取址。用户直觉通常是“先解引用数组，再取第 0 个元素”。这两种语义在非 0 下标时会产生完全不同的地址，属于潜在 wrong-code。

修复：

- `src/sema/sema_expr.cpp`：如果被下标对象是 pointer，且 pointee 是 array，直接报错：`indexing pointer to array requires explicit dereference`。
- 调用方必须写成 `(*values)[0]`，这样语义明确是 array indexing，而不是 pointer arithmetic。

回归：

- `tests/samples/negative/pointers/index_pointer_to_array.ax`

验证：

- 负例在 sema 阶段失败。
- 既有 pointer / array 正例继续通过。

### 10.15 限定泛型类型推断忽略 alias scope

问题：

泛型函数参数类型模式中，`ga::Box<T>` / `ga::Choice<T>` 这类限定泛型类型在推断时仍走“可见模块查找 `Box` / `Choice`”路径。只要同一作用域同时 import 另一个模块的同名泛型类型，就会误报 ambiguous，甚至有绑定到错误模板的风险。

示例：

```aurex
import samplelib.generic_a as ga;
import samplelib.generic_b as gb;

fn read_box<T>(box: ga::Box<T>) -> T {
    return box.value;
}
```

旧逻辑会把 `ga::Box<T>` 当成未限定的 `Box<T>` 参与泛型推断，看到 `generic_a.Box` 和 `generic_b.Box` 后报 ambiguous。

修复：

- `src/sema/generic.cpp`：`infer_generic_args_from_type_pattern` 在 named generic pattern 分支消费 `pattern.scope_name`：
  - 有 alias 时先 `resolve_import_alias`。
  - struct template 用 `find_generic_struct_template_in_module`。
  - enum template 用 `find_generic_enum_template_in_module`。
  - 没有 alias 时才走 visible-module lookup。

回归：

- `tests/samples/positive/generics/qualified_generic_inference_import.ax`
- `tests/gtest/sema/generics_tests.cpp`

验证：

- `build/bin/aurexc -I tests/samples/imports --check tests/samples/positive/generics/qualified_generic_inference_import.ax`
- `--emit=checked` 中同时存在 `samplelib.generic_a.Box<i32>` 和 `samplelib.generic_b.Box<i32>`，但实例化函数参数绑定到 `samplelib.generic_a.*`。
- 单文件编译运行通过。
- `build/bin/aurex_tests --gtest_filter=AurexIntegrationTest.QualifiedGenericInferenceUsesAliasScope`

### 10.16 symlink import 命中 canonical cache 后跳过 module-name 校验

问题：

模块 loader 对同一 canonical file path 建缓存。旧逻辑在缓存命中时直接返回已加载 module id，没有重新校验当前 import path 期望的 module name。

复现形态：

```text
imports_symlink/a.ax      # 内容声明 module a
imports_symlink/b.ax -> a.ax
```

```aurex
import a as alpha;
import b as beta;
```

如果先加载 `a`，再加载 symlink `b`，`b.ax` canonical 后命中 `a.ax` 缓存，旧 loader 会静默把 `beta` 也绑定到 `module a`。

修复：

- `src/driver/module_loader.cpp`：`loaded_file_modules_` 缓存命中时，如果调用方提供了 `expected_module`，必须用已加载 module path 再跑一次 `module_paths_equal`。
- 如果声明名和当前 import 期望名不同，报：
  - `module declaration 'a' does not match import 'b'`

回归：

- `tests/gtest/integration/regression_tests.cpp`：测试里动态创建 `a.ax` 和 `b.ax -> a.ax`，避免依赖平台外部临时文件。

验证：

- `/private/tmp/aurex_bughunt/symlink_import_mismatch.ax` 现在在 module loading 阶段失败。
- `build/bin/aurex_tests --gtest_filter=AurexIntegrationTest.SymlinkedImportStillValidatesExpectedModuleName`

### 10.17 const 二元表达式三层支持不一致

问题：

此前 `const VALUE: i32 = 1 + 2;` 被 sema 判定为非编译期常量。即使放开 sema，IR verifier 和 LLVM constant initializer lowering 也没有 `ValueKind::binary` 的常量路径，会退回 `undef` 或报 `constant initializer is not compile-time constant`。

修复：

- `src/sema/sema_decls.cpp`：
  - 放开可稳定折叠的二元 const 表达式：`+`、`-`、`*`、比较、相等/不等、位运算。
  - 继续拒绝 `&&` / `||`，因为当前 lowering 需要短路 CFG，不是单个 constant value。
  - 暂不放开 `/`、`%`、shift，避免 const ref 组合绕过更严格的除 0 / shift poison 检查。
- `src/ir/verify.cpp`：
  - 常量 initializer 允许 `ValueKind::binary`。
  - 复用 binary verifier 校验 operand/result 合法性。
  - 递归要求左右操作数也都是 compile-time constant。
- `src/backend/llvm/llvm_backend_value.cpp`：
  - 新增 `emit_constant_binary`。
  - 使用 LLVM constant fold 生成 integer/float arithmetic、comparison、bitwise constant。
- `src/backend/llvm/llvm_backend_internal.hpp`：声明新增 helper。

回归：

- `tests/samples/positive/types/const_binary.ax`
- `tests/samples/negative/types/const_initializer_binary_expr.ax` 改为覆盖 `true && false` 仍不能作为 const initializer。

验证：

- `--dump-llvm-ir tests/samples/positive/types/const_binary.ax` 中：
  - `@m0_const_binary_SUM = ... constant i32 3`
  - `@m0_const_binary_PRODUCT = ... constant i32 21`
  - `@m0_const_binary_ORDERED = ... constant i1 true`
- 正例单文件编译运行通过。
- `true && false` 负例仍在 sema 阶段失败。

### 10.18 rvalue / const aggregate 字段读取被错误要求为 place

问题：

旧 sema 把普通字段读取和字段地址投影混在一起处理，要求非 pointer receiver 必须是 place。这会错误拒绝：

```aurex
fn make() -> S {
    return S { value: 7 };
}

return make().value;
```

以及：

```aurex
const S_VALUE: S = S { value: 1 };
return S_VALUE.value;
```

这两种都是只读字段访问，不需要用户侧有可写/可取址 place。真正需要 place 的场景是赋值左侧、`&expr.field`、`*mut self` receiver 等。

修复：

- `src/sema/sema_expr.cpp`：
  - 字段读取不再要求非 pointer object 必须是 place。
  - 仍然校验 object 必须是非 opaque struct 或 pointer-to-struct。
  - private field access 规则不变。
- `src/ir/lower_ast_expr.cpp`：
  - `lower_object_place_or_value` 先尝试真实 place。
  - 如果 object 是 rvalue，就把 object lowering 到临时 `alloca field.object`，`store` 后再 `field_addr`。
  - 返回的字段地址对 rvalue receiver 视为 const，写入路径仍不能借此绕过 mutability。

回归：

- `tests/samples/positive/expressions/rvalue_struct_field_access.ax`
- `tests/samples/positive/expressions/const_struct_field_access.ax`
- 原负例 `tests/samples/negative/expressions/rvalue_struct_field_access.ax` 改为覆盖 `make().value = 1` 仍非法。
- 原负例 `tests/samples/negative/expressions/const_struct_field_access.ax` 改为覆盖 `S_VALUE.value = 2` 仍非法。

验证：

- 两个正例 `--check`、`--dump-ir`、单文件编译运行均通过。
- IR 中可见：
  - `alloca field.object`
  - `store` rvalue/const aggregate 到临时槽
  - `field_addr` 后 `load`
- 两个负例仍在 sema 阶段报 `left side of assignment must be writable`。

### 10.19 rvalue receiver 调用 `*const self` 方法被误拒绝

问题：

旧 `method_receiver_matches` 对所有 pointer self 都要求 receiver 是 place，因此会拒绝：

```aurex
Counter.new(7).read()
```

其中 `read(self: *const Counter)` 只需要临时只读地址。这个限制对 `*mut self` 是必要的，但对 `*const self` 是误伤。

修复：

- `src/sema/sema_lookup.cpp`：
  - `*mut self` 仍必须是 place，并且必须 writable。
  - `*const self` 只要求 receiver type 和 pointee type 匹配，不再要求 place。
- `src/ir/lower_ast_expr.cpp`：
  - lowering 方法 receiver 时，如果 self 参数是 pointer 且 receiver 不是 pointer：
    - `*mut self` 继续走 `lower_place_addr`。
    - `*const self` 走 `lower_object_place_or_value`，必要时 materialize 到临时槽后传 const pointer。

回归：

- `tests/samples/positive/functions/method_rvalue_const_receiver.ax`
- `tests/samples/positive/generics/generic_method_rvalue_const_receiver.ax`
- 继续保留并验证：
  - `tests/samples/negative/functions/method_receiver_not_place.ax`
  - `tests/samples/negative/functions/method_receiver_mutability.ax`

验证：

- 普通方法和泛型方法正例 `--check`、`--dump-ir`、单文件编译运行均通过。
- IR 中 rvalue receiver 先 `alloca field.object`，再 `pcast` 为 `*const Counter` / `*const Box<i32>`。
- `Counter.new(1).add(2)` 仍报 `method receiver must be a place expression`。
- immutable local 调 `*mut self` 仍报 `mutable method receiver requires writable storage`。

### 10.20 `field_addr` / `index_addr` verifier 漏检结果地址类型

问题：

IR verifier 只检查了 `field_addr` / `index_addr` 的 object 是 pointer、字段存在、index id 存在，但没有检查：

- result 本身必须是 pointer。
- result pointee 必须等于字段类型或元素类型。
- 不能从 `*const T` object 产生 `*mut field` / `*mut element` 地址。
- index value 必须是 integer。

这会让 IR pass 可以构造出类型不匹配的地址投影，直到 LLVM backend 或更晚阶段才暴露。

修复：

- `src/ir/verify.cpp`：
  - `verify_field_addr` 校验 result pointer、pointee type、const-to-mut 投影。
  - 新增 `verify_index_addr`，校验 result pointer、index integer、元素 pointee type、const-to-mut 投影。

回归：

- `tests/gtest/ir/ir_verifier_structural_tests.cpp`

验证：

- `build/bin/aurex_tests --gtest_filter=CoreUnit.IrVerifierReportsRepresentativeStructuralErrors`
- 新增坏 IR 分别覆盖：
  - field address result type mismatch
  - field address mutable through const object
  - index address result type mismatch
- 新增 rvalue field / const receiver 正例的 `--dump-ir` 均通过 verifier。

### 10.21 `load` / `store` / `alloca` / literal verifier 兜底不完整

问题：

IR verifier 仍有几类基础 Value 形状只做了“类型句柄存在”检查，没有校验语义约束：

- `load` 没有检查 result type 必须等于 object pointee type。
- `store` 没有检查 target pointer 必须是 `*mut T`。
- `alloca` 没有检查 result 必须是 `*mut storage_type`。
- `null`、`bool`、`byte`、string literal 等没有检查自身 type 是否匹配 literal kind。
- `size_of` / `align_of` 没有检查 result 必须是 `usize`，target 必须是合法 storage type。

这些坏 IR 不一定来自正常前端路径，但系统级语言编译器必须在 verifier 层兜住，否则 pass bug 会继续流到 LLVM，表现为后端 assert、非法 IR 或 silent wrong-code。

修复：

- `src/ir/verify.cpp`：
  - 新增 `verify_literal_value`，校验 literal kind 和 type 的一致性。
  - 新增 `verify_alloca`，要求 `alloca` 结果是 `*mut`，且 pointee 是可存储类型。
  - 新增 `verify_load`，要求 load result 非 void 且等于 object pointee。
  - `store` 额外要求 target pointer 是 mutable。
  - 新增 `verify_size_or_align`，要求 result 是 `usize`，target 是可存储类型。
  - const initializer 路径也复用 literal / size_of / align_of 校验。

回归：

- `tests/gtest/ir/ir_verifier_structural_tests.cpp`

验证：

- `cmake --build build --target aurex_tests`
- `build/bin/aurex_tests --gtest_filter=CoreUnit.IrVerifierReportsRepresentativeStructuralErrors`
- `build/bin/aurex_tests --gtest_filter=CoreUnit.IrVerifierReportsAdditionalEdgeCaseErrors`
- `git diff --check`

### 10.22 `*mut void` / opaque pointer 可被解引用或下标

问题：

`void` 和 opaque struct 可以作为 pointer pointee 使用，但不能作为 by-value storage 被读取。旧 sema 只检查 `*expr` 的 operand 是否是 pointer，然后直接把结果类型设成 pointee。这会错误接受：

```aurex
let pointer: *mut void = paddr(*mut void, 0);
return *pointer;
```

以及：

```aurex
extern c {
    opaque struct Handle;
}

let handle: *mut Handle = paddr(*mut Handle, 0);
return *handle;
```

下标也有同类问题：

```aurex
let pointer: *mut void = paddr(*mut void, 0);
let value = pointer[0];
```

这些路径会让后续 lowering 试图生成 `load void` 或 load unsized opaque value。即使 verifier 能兜住，正确行为也应该是在语义分析阶段以用户代码错误报出。

修复：

- `src/sema/sema_expr.cpp`：
  - dereference operand 是 pointer 后，继续要求 pointee 是 `is_valid_storage_type`。
  - pointer indexing 在排除 pointer-to-array 后，也要求 pointee 是合法 storage type。
  - 对非法 dereference 报 `dereference requires pointer to valid storage`。
  - 对非法 pointer indexing 报 `indexing pointer requires pointer to valid storage`。

回归：

- `tests/samples/negative/pointers/deref_void_pointer.ax`
- `tests/samples/negative/pointers/deref_opaque_pointer.ax`
- `tests/samples/negative/pointers/index_void_pointer.ax`

验证：

- `cmake --build build --target aurexc`
- 三个新增负例 `--check` 均在 sema 阶段失败。
- `tests/samples/positive/std/std_bootstrap.ax` 仍通过 `--check`，确认 `*mut void` 作为 handle 传递不受影响，只禁止读取其 pointee value。

### 10.23 全量测试暴露的 verifier / 测试编排回归

问题：

全量 `build/bin/aurex_tests` 在补强 verifier 后首次暴露 14 个失败，集中在三类：

- verifier 把 enum 判别值用的 `integer_literal` 误判为非法，因为 `TypeTable::is_integer` 不包含 enum 类型。
- 少量 backend/pass whitebox 测试手写了 sema 不会生成的非法 IR，补强 verifier 后这些测试不应再假装是合法输入。
- 泛型私有类型诊断路径在“先查 struct 再查 enum”时误报 `unknown generic enum`。
- 正例 sample suite 对带 import 的普通正例没有统一加 `tests/samples/imports` import path，新增 qualified generic 样本后暴露出来。

修复：

- `src/ir/verify.cpp`：
  - 新增 enum-aware integer literal type 检查。
  - enum 判别值 literal 不再被误报为 `integer literal type must be integer`。
- `src/sema/sema_types.cpp`：
  - unqualified generic type 参数解析中，试探 enum template 时不再提前报告 `unknown generic enum`。
  - 统一落到更准确的 `type arguments require a generic type: Name` 或后续具体 struct literal 诊断。
- `tests/gtest/backend/llvm_runtime_tests.cpp`：
  - 移除非法 struct equality 手写 IR，改用合法 pointer equality 覆盖 backend runtime equality lowering。
- `tests/gtest/ir/pass_pipeline_tests.cpp`：
  - 对专门测试 pass 容忍非法中间形态的用例显式关闭 output verifier。
  - 把其他可合法化的手写 IR 调整为 verifier 可接受的形态。
- `tests/gtest/integration/sample_suite_tests.cpp`：
  - 正例 sample suite 编译普通样本时统一传入 `tests_import_flags()`。
- `tests/gtest/sema/generics_tests.cpp`：
  - 更新私有 generic struct / enum 负例的诊断断言，匹配当前更稳定的实际错误。

验证：

- 第一次全量：`91` 个测试中 `77` 通过、`14` 失败。
- 修复后定向复跑：
  - `CoreUnit.LlvmBackendCoversPhiRuntimeCastsUnaryBinaryAndConstantInitializers`
  - `CoreUnit.PassPipelineRewritesAggregatePhiAndConstantsAfterMem2Reg`
  - `CoreUnit.PassPipelineCoversNonPromotableEscapeAndInvalidValueTolerance`
  - `CoreUnit.PassPipelineSkipsEmptyBranchMergeWhenTargetHasPhi`
  - `AurexIntegrationTest.GenericImportVisibilityAndAmbiguityDiagnostics`
  - `AurexIntegrationTest.SampleSuite_PositiveSamples`
- 最终全量：`build/bin/aurex_tests`
  - `91` 个测试全部通过。
  - gtest 总耗时约 `37338 ms`。
