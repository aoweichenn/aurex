# Aurex M5 Default Trait Methods Release Baseline

状态：已完成。

日期：2026-05-31。

M5 在已经收口的 M4 trait / protocol 系统之上，把 default trait methods 收口为 release baseline。本阶段新增
trait-owned default method body、显式 override vs inherited-default origin fact、用于静态 lowering 的 concrete
default-body instance，以及 tooling / diagnostics / incremental-cache 投影，同时保持 M4 static-dispatch-first
架构。

## 发布范围

M5 的语言表面刻意保持窄边界：

```aurex
trait Reader {
    fn read(self: &Self) -> i32;

    fn is_empty(self: &Self) -> bool {
        return self.read() == 0;
    }
}

struct File {
    value: i32;
}

impl Reader for File {
    fn read(self: &File) -> i32 {
        return self.value;
    }
}

fn check(file: &File) -> bool {
    return file.is_empty();
}
```

本次发布契约是：

- trait method requirement 可以是 prototype，也可以带 body。
- 带 body 的 requirement 是 defaulted requirement，impl 可以省略。
- 不带 body 的 requirement 仍然必须由每个 concrete impl 实现。
- 签名替换后匹配的 impl method 是显式 `impl_override`。
- 省略的 defaulted requirement 会记录为 `trait_default`。
- generic body 继续通过 `param_env` 绑定，并在实例化时重新选择 override 或 default。
- default body 在 trait context 中以抽象 `Self`、trait generics、where predicates 和 associated-type projection
  进行 type-check。
- 被选中的 inherited default 会 materialize 为 concrete `TraitDefaultMethodInstanceInfo` 记录，并降低成 internal
  direct-call function。
- `BodySlotKind::trait_default_method` 是 trait-owned default body 和 concrete default instance 的稳定 body identity。
- inherent method 仍优先于 trait method；已有 M4 ambiguity 规则继续拒绝歧义 trait candidate。

## 编译器边界审计

| 边界 | M5 release 契约 |
| --- | --- |
| Lexer/parser/AST | trait body 同时接受 `fn requirement(...);` 和 `fn requirement(...) { ... }`；AST node 区分 prototype requirement 和 trait default method。 |
| Query identity | default method body 使用 `BodySlotKind::trait_default_method`；selected default instance 的 stable name、symbol 和 incremental key 由 trait、impl、receiver、trait args 和 associated-type outputs 推导。 |
| Sema | `TraitMethodRequirement` 记录 default-body metadata；`TraitImplInfo` 记录 `impl_override` vs `trait_default`；`TraitMethodCallBinding` 记录 `impl_override`、`trait_default` 或 `param_env`；default body 在 trait context 中检查一次，只有被选中时才在 retained side tables 下形成 concrete instance。 |
| IR/backend | trait 内原始 default method item 不作为普通 function lowering；retained concrete default instance lowering 为 internal static function，selected call lowering 为 direct call。M5 不引入 vtable、trait object ABI 或 dynamic dispatch。 |
| Driver/cache/profile | incremental-cache subjects 输出 `trait_default_method_instance` 行和 selected default 的 body-query 边，且不会把 synthetic default instance 重复记录为普通 `function`。 |
| Tooling/LSP | IDE hover/definition 把 inherited default call 跳到 trait method requirement，把 explicit override call 跳到 impl method；synthetic default instance function 不暴露为可见全局符号。 |
| Diagnostics | default-body error 指向 trait source；显式 override 签名错误仍按 requirement mismatch 诊断，并补充说明 default body 不会放宽 override 签名规则。 |

## 验证矩阵

M5 release 验证使用常规仓库测试，不使用临时 fixture。

| 范围 | 覆盖 |
| --- | --- |
| Parser / AST / dumps | `tests/gtest/frontend/parser_tests.cpp`、`tests/gtest/frontend/ast_dump_tests.cpp` |
| Query body identity | `tests/gtest/query/query_key_tests.cpp` |
| Sema whitebox 和 checked dump | `tests/gtest/sema/trait_tests.cpp` |
| IR lowering guard 和 concrete default body lowering | `tests/gtest/ir/lower_ast_whitebox_tests.cpp` |
| IDE hover/definition/body-query 行为 | `tests/gtest/tooling/ide_tooling_tests.cpp` |
| Incremental-cache rows 和 query edges | `tests/gtest/driver/cli_driver_tests.cpp` |
| Checked-origin fixtures | `tests/samples/checked/traits/trait_default_method_inherited.ax`、`tests/samples/checked/traits/trait_default_method_override.ax` |
| 正向 lowering/native 样例 | `tests/samples/positive/traits/trait_default_method_*.ax` |
| 负向诊断 | `tests/samples/negative/traits/trait_default_method_*.ax` |
| 文档稳定性 | `tests/gtest/integration/documentation_tests.cpp` |

必跑收口门禁：

```sh
tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')
git diff --check
cmake --build cmake-build-release -j4
ctest --test-dir cmake-build-release --output-on-failure -j4
tools/check_coverage.sh -j4
make perf-stress-threshold
```

更重的 scheduled/manual `make perf-release-threshold` 和 `make query-sanitizer` 仍保留给大型 release lane。
它们是质量门禁，不代表 M5 扩大语言表面。

## Unsupported Matrix

| 能力 | M5 状态 | 原因 | 后续入口 |
| --- | --- | --- | --- |
| `dyn Trait`、trait object、object safety | 不支持 | 需要 receiver、associated item、default method 的 object-callability 规则和稳定 data/vtable layout。 | dynamic trait / object-safety 设计。 |
| vtable ABI / dynamic dispatch | 不支持 | M5 明确把 inherited default 保持为静态选择的 direct call。 | dynamic trait ABI 设计。 |
| specialization / overlapping defaults | 不支持 | 需要 partial ordering、semver-aware overlap 和独立于简单 default inheritance 的 dispatch selection 规则。 | specialization 设计。 |
| default associated type | 不支持 | 需要 associated-type default selection、override rule、equality interaction 和 cycle/depth control。 | associated type default 设计。 |
| associated const | 不支持 | 需要 const-eval authority 和 associated-item equality/fingerprint 规则。 | associated item 扩展。 |
| generic associated type | 不支持 | 需要更强 solver、higher-ranked reasoning 和 projection normalization depth control。 | solver / GAT 设计。 |
| minimal implementation annotation | 不支持 | 需要类似 GHC `MINIMAL` 的用户可见 dependency set 和诊断，且不能改变 default inheritance 语义。 | minimal requirement 设计。 |
| blanket impl / package-level coherence expansion | 不支持 | 会改变 candidate discovery、overlap 和 semver 边界，超出 M4/M5 exact impl fact。 | coherence / package 设计。 |
| RAII、`Drop`、`Copy`、move-only value | 不支持 | 资源语义影响 method receiver、generic bound、drop timing、cleanup lowering 和 ABI。 | resource semantics 设计。 |
| borrow checker / lifetime | 不支持 | 需要独立 aliasing 和 region model。 | resource / borrow 设计。 |
| Swift-style protocol extension | M5 拒绝 | extension default 容易制造 static-vs-dynamic dispatch 惊讶和 member identity 歧义。 | 当前无入口。 |
| Scala/Kotlin mixin state 或 linearization | M5 拒绝 | 需要 state、initialization、`super<Trait>` 和 linearization 规则，超出 Aurex static trait。 | 如果未来选择 class/composition，再单独设计。 |
| C++ arbitrary requires-expression | M5 拒绝 | 对当前 canonical predicate 和 trait evidence model 过宽。 | 未来 constraint-system 设计。 |

## 发布结论

M5-WP1 到 M5-WP7 已全部完成。当前分支已经形成一致的 default trait method baseline：

- WP1 完成调研和设计。
- WP2 完成 syntax、AST 和 stable default-body identity。
- WP3 完成 trait-context default body type checking。
- WP4 完成 impl completeness 和显式 method-origin fact。
- WP5 完成 static lowering、backend 和 monomorphization 行为。
- WP6 完成 tooling、diagnostics 和 incremental-cache projection。
- WP7 完成发布文档、unsupported 边界、常规仓库测试、coverage、query/cache gates 和 stress gates 收口。

M5 之后的工作必须作为独立设计流启动。最强候选是 resource semantics、dynamic trait object、
specialization、default associated type、associated const、minimal implementation annotation、package-level
coherence 或更强 trait solver。这些都不应该重新打开 M5 static default-method baseline。
