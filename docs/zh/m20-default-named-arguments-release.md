# Aurex M20g Default And Named Call Arguments Closure Release Baseline

日期：2026-06-11
阶段：M20g Default And Named Call Arguments Closure
状态：已完成

## 范围

M20g 补齐默认参数和命名参数的第一版 release-quality 用户面。该阶段只做语言前端、checked facts、
IR lowering 和既有 borrow/place/move 分析的闭环，不实现任何标准库，不新增 ABI 级 optional parameter metadata，
也不改变函数真实签名。

当前支持：

- 普通函数参数默认值。
- inherent method 参数默认值。
- 泛型函数和 owner generic method 的默认参数。
- 普通函数 call-site named argument。
- inherent method call-site named argument。
- trait/static/dyn method call-site named argument，只要目标 method metadata 中有参数名。
- 默认值填充和命名实参重排后的 checked `ordered_args`。
- IR lowering、borrow summary、body flow graph、body loan precheck、place-state precheck、move analysis、
  borrow escape / lambda capture 扫描对 checked `ordered_args` 的统一消费。

当前不支持：

- 标准库 API。
- ABI 级 default / optional parameter metadata。
- trait requirement 默认参数。
- C ABI / variadic 函数默认参数。
- variadic C call 的命名参数。
- function value / lambda 间接调用的命名参数或默认参数。
- enum constructor payload 命名参数。
- 默认表达式引用前序参数。
- overload resolution 中使用 named argument 做候选消歧。

## 用户语法

```aurex
module m20g.default_named_arguments;

fn mix(a: i32, b: i32 = 10, c: i32 = 100) -> i32 {
    return a + b + c;
}

struct Acc {
    base: i32;
}

impl Acc {
    fn add(self: &Acc, value: i32, scale: i32 = 1) -> i32 {
        return self.base + value * scale;
    }
}

fn main() -> i32 {
    let acc: Acc = Acc { base: 5 };
    let one: i32 = mix(1);
    let two: i32 = mix(1, 2);
    let three: i32 = mix(a: 1, c: 3);
    let four: i32 = mix(1, c: 3);
    let five: i32 = acc.add(value: 2);
    let six: i32 = acc.add(scale: 3, value: 2);
    return one + two + three + four + five + six;
}
```

规则：

- 默认参数写作 `name: Type = expr`。
- required 参数不能出现在 default 参数之后。
- 默认表达式按参数类型做 expected-type 分析。
- 默认表达式当前不捕获前序参数作用域。
- named 参数之后不能再写 positional 参数。
- named 参数必须匹配目标参数名，重复或未知名称会诊断。
- 缺失的 required 参数会诊断；缺失的 defaulted 参数由声明默认值填充。

## 语义模型

M20g 将默认参数和命名参数定义为 source-level call sugar：

1. Parser 在函数参数 AST 上保存 `default_value`，在 call expression 上保存 `arg_labels`。
2. Sema 解析 call 时读取目标 `FunctionParamInfo`，先按参数表归一化实参。
3. Positional 参数按源顺序填入下一个未填 required/defaulted 参数位。
4. Named 参数按参数名查找目标参数位，并拒绝重复、未知和 variadic named call。
5. 缺失 defaulted 参数用声明中的 `default_value` 补齐。
6. 所有 fixed 参数按声明顺序进行类型检查，variadic tail 仍保留在 fixed 参数之后。
7. Checked binding 保存 `ordered_args`，作为后续阶段唯一可靠的 fixed argument 顺序。

这个模型的核心约束是：source `args` 只表示用户书写顺序，checked `ordered_args` 表示语义调用顺序。任何会影响
求值、借用、move 或 lowering 的阶段必须读取 checked order。

## 实现入口

Parser / AST：

- `include/aurex/frontend/syntax/ast/nodes.hpp`
- `include/aurex/frontend/syntax/ast/expr_nodes.hpp`
- `include/aurex/frontend/syntax/ast/module.hpp`
- `src/frontend/parse/grammar/parser_fn.cpp`
- `src/frontend/parse/grammar/parser_postfix.cpp`
- `src/frontend/syntax/ast/dump.cpp`

Sema / checked facts：

- `include/aurex/frontend/sema/function.hpp`
- `include/aurex/frontend/sema/checked_module.hpp`
- `include/aurex/frontend/sema/function_registry.hpp`
- `include/aurex/frontend/sema/call_arguments.hpp`
- `src/frontend/sema/facade/sema_call.cpp`
- `src/frontend/sema/internal/declarations/sources/sema_declaration_analyzer.cpp`
- `src/frontend/sema/internal/declarations/sources/sema_generic_analyzer.cpp`
- `src/frontend/sema/internal/traits/sources/sema_trait_analyzer.cpp`

Lowering / analyses：

- `src/midend/ir/lowering/sources/lower_ast_expr.cpp`
- `src/frontend/sema/internal/borrow/sources/summary.cpp`
- `src/frontend/sema/internal/borrow/sources/flow_graph.cpp`
- `src/frontend/sema/internal/borrow/sources/loan_checker.cpp`
- `src/frontend/sema/internal/place/sources/move_analysis.cpp`
- `src/frontend/sema/internal/place/sources/place_state.cpp`
- `src/frontend/sema/internal/expressions/sources/sema_statement_analyzer.cpp`

`include/aurex/frontend/sema/call_arguments.hpp` 是公开 helper，不放在 frontend internal private 目录下，因为
midend lowering 需要读取 checked ordered call arguments，同时不能依赖 frontend 私有头。

## 诊断矩阵

已固定的错误面：

- required 参数跟在 default 参数之后。
- C ABI / variadic 函数声明默认参数。
- trait requirement 声明默认参数。
- named 参数未知。
- named 参数重复。
- named 参数后出现 positional 参数。
- 缺失 required 参数。
- enum constructor 使用 named argument。
- function value / lambda indirect call 使用 named argument。
- variadic C call 使用 named argument。

## 验证

新增和更新的关键测试：

- `tests/gtest/frontend/parse/parser_tests.cpp`：
  `ParserParsesDefaultParametersAndNamedCallArguments`
- `tests/gtest/frontend/sema/default_named_argument_tests.cpp`：
  默认值记录、checked ordered binding、泛型调用、trait method named call、无效声明、无效调用、borrow ordered args
- `tests/gtest/integration/native_execution_tests.cpp`：
  `NativeDefaultAndNamedArgumentsUseCheckedOrder`

覆盖重点：

- parser 保留参数默认值和 call-site label。
- checked dump 显示参数默认值和 `ordered_args`。
- 泛型函数调用会经过默认参数填充和 named argument 重排。
- trait method named argument 可以进入 checked trait method binding。
- borrow checker 使用 ordered args，命名重排不会让 returned-origin / active loan 冲突判断回到源码顺序。
- native execution 按参数声明顺序传参。

## 与标准库的关系

本阶段不实现标准库，也不需要标准库才能验证。所有样例和测试都使用自包含 `.ax` source。

后续如果要把 default / named argument 暴露为库 API 的稳定 ABI 或反射元数据，必须单独设计：

- 参数名是否进入 public ABI 或只保留在 source/checker metadata。
- 默认表达式是否允许跨模块调用方重放。
- 默认表达式是否需要 comptime / const-eval。
- function value / lambda 是否携带参数名。
- C ABI 是否永远拒绝默认值，或只在 Aurex wrapper 层提供 source sugar。

M20g 暂时选择最保守路线：默认值和参数名只服务 Aurex source-level call checking，不改 ABI。
