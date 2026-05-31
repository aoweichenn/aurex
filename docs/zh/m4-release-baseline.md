# Aurex M4 Trait / Protocol Release Baseline

状态：已完成。

日期：2026-05-31。

M4 把 Aurex 第一版 trait / protocol 系统收口为 release baseline。本阶段新增
nominal static trait、显式 trait impl、generic trait predicate、静态 trait method
分派、associated type，以及 tooling / diagnostics 投影，同时保持 M3 已经建立的
query-backed 编译器架构边界。

## 发布范围

M4 的语言表面刻意保持窄边界：

```aurex
trait Source {
    type Item;
    fn get(self: &Self) -> Self.Item;
}

struct Bytes {
    value: i32;
}

impl Source for Bytes {
    type Item = i32;

    fn get(self: &Bytes) -> i32 {
        return self.value;
    }
}

fn read_i32[T](value: &T) -> i32 where T: Source[Item = i32] {
    return value.get();
}
```

本次发布契约是：

- `trait` 是 nominal definition，拥有稳定 `DefKey`。
- conformance 必须通过显式 `impl Trait for Type` 给出。
- trait impl 必须满足所有 method 和 associated type requirement。
- generic `where T: Trait` 会降低为当前 `ParamEnv` 中的 canonical trait predicate。
- `Sized`、`Eq`、`Ord`、`Hash` 保持兼容用的 capability 检查，同时记录 compiler-owned builtin trait predicate
  fact。
- trait method call 只走静态分派。generic body 通过 `ParamEnv` 绑定；concrete receiver 绑定到唯一可见
  impl method；单态化后 lowering 生成 direct call。
- associated type 是 impl output，不参与 impl selection input。
- `Trait[Item = Type]` 在 trait predicate 上记录 associated-type equality constraint。
- `Self.Item` 和 generic associated-type projection 会降低为 canonical projection type。

## 编译器边界审计

| 边界 | M4 release 契约 |
| --- | --- |
| Lexer/parser/AST | `trait`、trait requirement、trait impl、associated type declaration、associated type assignment 和 where equality 都是结构化语法，不靠字符串解析。 |
| Query identity | trait、impl、trait method、associated type、obligation、evidence 和 method-call binding 使用 stable query key / member key。 |
| Sema | trait declaration、impl registry fact、coherence、orphan rule、first-pass overlap check、predicate lowering、candidate rejection、method resolution 和 associated-type equality diagnostics 都由 sema 持有。 |
| IR/backend | 静态 trait method call 降低为具体 impl-method direct call。M4 不生成 vtable、trait object layout 或 drop glue。 |
| Driver/cache/profile | trait fact 进入 checked-module fingerprint 和既有 query/cache/profile gates，不新增 trait 专用 driver 路径。 |
| Tooling/LSP | IDE 和 ToolingSession 暴露 protocol-neutral `Ide*` / `Tooling*` value；LSP kind 只在 LSP adapter 内映射。 |
| Diagnostics | candidate impl、rejected candidate、associated-type actual value、orphan location 和 overlap location 以结构化 diagnostic/note 输出。 |

## 验证矩阵

M4 release 验证使用常规仓库测试，不使用临时 fixture。

| 范围 | 覆盖 |
| --- | --- |
| Sema whitebox 和 checked dump | `tests/gtest/sema/trait_tests.cpp` |
| 正向样例 | `tests/samples/positive/traits/*.ax` |
| 负向样例 | `tests/samples/negative/traits/*.ax` |
| 跨模块可见性 | `tests/samples/imports/samplelib/traits.ax` 和 `trait_facade.ax` |
| 静态分派 / LLVM / native 执行 | trait method 和 associated-type 正向样例通过 sample-suite 检查 |
| IDE/tooling/LSP 投影 | `tests/gtest/tooling/ide_tooling_tests.cpp`、`tests/gtest/tooling/session_lsp_tooling_tests.cpp` |
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

CI release lane 仍保留更重的 scheduled/manual `make perf-release-threshold` 和
`make query-sanitizer`，用于大规模 stress 和 sanitizer 覆盖。它们是发布质量门，不代表 M4 新增语言表面。

## Unsupported Matrix

| 能力 | M4 状态 | 原因 | 后续入口 |
| --- | --- | --- | --- |
| `dyn Trait`、trait object、object safety | 不支持 | 需要 data/vtable 表示、receiver compatibility、associated-item object rules 和稳定 ABI。 | dynamic trait / object-safety 设计。 |
| vtable ABI / dynamic dispatch | 不支持 | M4 明确只做 static dispatch。 | dynamic trait ABI 设计。 |
| RAII、`Drop`、`Copy`、move-only value | 不支持 | 资源语义会影响 ownership、drop timing、cleanup lowering、generics 和 ABI。 | resource semantics 设计。 |
| Borrow checker / lifetime | 不支持 | 需要独立 aliasing 和 region model。 | resource / borrow 设计。 |
| Default trait method | 不支持 | 需要 method-origin 规则、override 语义、冲突诊断和 codegen policy。 | M4 之后的 default-method 设计。 |
| Associated const | 不支持 | 需要 const-eval authority 和 associated-item equality 规则。 | associated item 扩展。 |
| Generic associated type | 不支持 | 需要更强 solver、higher-ranked reasoning 和 cycle/depth control。 | solver / GAT 设计。 |
| Specialization | 不支持 | 需要 semver-aware overlap、partial ordering 和 dispatch selection 规则。 | specialization 设计。 |
| Negative / unsafe / auto trait | 不支持 | 与 coherence、安全、资源和并发保证耦合。 | trait-safety / resource 设计。 |
| Go-style structural interface | M4 拒绝 | 会让 conformance 变成隐式，削弱 query identity/coherence。 | 当前无入口。 |
| Scala-style implicit/given search | M4 拒绝 | solver 成形前引入全局搜索和歧义压力。 | 如果未来需要，必须作为显式 contextual evidence 系统设计。 |
| C++ arbitrary requires-expression | M4 拒绝 | 对当前 canonical predicate model 过宽。 | 未来 constraint-system 设计。 |
| Package manager / version solver | 不支持 | 与 trait 语言表面正交。 | package / workspace 设计。 |

## 发布结论

M4-WP1 到 M4-WP8 已全部完成。当前分支已经形成一致的 trait / protocol baseline：

- WP1 完成调研和设计。
- WP2 完成 syntax、AST 和 query identity scaffold。
- WP3 完成 trait declaration 和 impl registry。
- WP4 完成 coherence 和 generic predicates。
- WP5 完成 static trait method resolution and lowering。
- WP6 完成第一版 associated type model。
- WP7 完成 tooling and diagnostics projection。
- WP8 完成发布文档、unsupported 边界和验证门禁收口。

M4 之后的工作必须作为独立设计流启动。最强候选是 resource semantics、dynamic trait object、
package-level coherence、default methods / specialization、class-like sugar 或更强 trait solver。这些都不应该重新打开
M4 static trait baseline。
