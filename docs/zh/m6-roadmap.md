# M6 资源、值生命周期与访问语义路线图

## 阶段定位

M6 建立在已经收口的 M5 static default-method baseline 上。M6 不重新打开 M4/M5 trait dispatch 决策，也不把
borrow checker、dynamic trait object、标准库重建和 array ABI 解禁混成一个巨型阶段。

完整设计基线见
[Aurex M6 资源、值生命周期与访问语义调研和三轮设计审视基线](m6-resource-access-semantics-design.md)。

M6 的目标是形成资源和值生命周期的 release-quality 基线：

```text
resource summary
-> whole-local move
-> CFG-sensitive initialized state
-> deterministic cleanup
-> aggregate / generic drop glue
-> tooling / query / cache
-> M7 borrow checker entry
```

## M6-WP1：三轮调研和设计审视

状态：已完成。

交付：

- 跨语言证据矩阵：C++、Rust、Swift、Mojo、Move、Zig、Go、Hylo、Pony、Verona、Cyclone、Lean、
  Koka、Roc、Linear Haskell、Idris 2、Austral、Carbon 和 Clang。
- 前沿研究审视：region、linear / quantitative types、reference capability、RC reuse、formal ownership
  semantics 和关系化 borrow analysis。
- Aurex 四维资源模型：`Copy`、`Discard`、`NeedsDrop`、future `MustConsume`。
- whole-local move 第一版边界。
- cleanup action stack、`defer` 组合、overwrite 顺序、destructor protocol、FFI 和 `unsafe` 规则。
- 用户案例和反例压力测试。
- 中英文设计文档、路线图、导航和 documentation integration test。

风险控制：

- 先固定语义，再设计 destructor parser spelling。
- 不把 `Drop` 降级成普通用户 trait。
- 不把 M6 误扩成 Rust 全量 borrow checker。

## M6-WP2：Resource Classification Scaffold

状态：已完成。

交付：

- 增加 compiler-owned `Copy` capability，并保持 `Drop` 不作为用户可写 bound。
- 增加内部四维资源摘要：`Copy` / `MoveOnly`、`Discard` / `MustConsume`、`Trivial` / `NeedsDrop`、owned / borrowed / raw / shared ownership。
- 对 builtin、pointer、reference、slice、`str`、tuple、array、struct、enum、generic param、associated projection 和 opaque struct 做结构化或保守分类。
- 增加 stable resource fingerprint 和 checked dump 中的 deterministic resource summary。
- 保持当前 `Drop` 用户 bound 拒绝，直到 destructor protocol surface 单独落地。

验收：

- sema whitebox 覆盖结构化分类、`Copy` capability 和 fingerprint。
- checked dump 覆盖 deterministic resource summary。
- 正常仓库样例覆盖 `where T: Copy` 正向路径，并删除旧的 `Copy` 拒绝负样例。
- `Drop` 用户 bound 继续由负样例确认拒绝。

## M6-WP3：Owned Use Mode 和 Whole-Local Move Analysis

状态：已完成。

交付：

- 给 expression use 增加 `owned_copy`、`owned_consume`、`shared_borrow`、`mutable_borrow` 和 `place_only` checked side-table fact。
- 增加独立 body move analysis 模块，使用迭代式 CFG build 和 worklist dataflow 实现 initialized / moved / maybe-moved。
- 支持 move 后重新初始化。
- diagnostics 指向 consume origin。
- 第一版只允许 whole-local move。

已用正式负样例明确拒绝：

- partial field move。
- indexed move-out。
- consuming pattern payload。
- non-`Copy` payload `?`，直到 aggregate transfer WP 证明完整 cleanup。

## M6-WP4：Cleanup Obligations、`defer` 组合和 IR Elaborator

状态：已完成。

目标：

- 建立 lexical cleanup action stack。
- 覆盖正常 scope exit、overwrite、`return`、`break`、`continue` 和 `?` early return。
- static / dead / conditional drop 分类。
- drop flag lowering。
- 正式 IR cleanup 节点或等价 CFG 形状。
- verifier 检查 glue target、place type、flag 和 double-elaboration invariant。

风险控制：

- named locals 保持 lexical cleanup，不做默认 ASAP destruction。
- deferred call 在退出时求值；deferred use-after-move 必须报错。

## M6-WP5：Destructor Protocol 和 Aggregate / Generic Drop Glue

状态：待开始。

目标：

- 在单独 parser 审视后固定 destructor surface spelling。
- nominal 类型最多一个 sealed lifecycle destructor body。
- struct、tuple、enum、array 和 generic instance drop glue。
- custom destructor body 后按规则递归 drop fields。
- aggregate 分步初始化失败回滚。
- 逐步开放已证明正确的 non-`Copy` enum payload transfer 和 `?`。

明确不做：

- destructor overload resolution。
- 用户显式 destructor 调用。
- unwind cleanup。
- managed global destruction。
- array-containing by-value ABI 全量解禁。

## M6-WP6：Tooling、Query、Cache 和性能闭环

状态：待开始。

目标：

- IDE hover 暴露 `Copy` / `MoveOnly` 和 `NeedsDrop`。
- move origin、cleanup origin 和 destructor definition projection。
- destructor `BodyKey`、drop glue key 和 body resource-check fingerprint。
- incremental invalidation tests。
- generic、CFG、diagnostics 和 cleanup stress lanes。
- 新增代码覆盖率至少 `95%`。

## M6-WP7：Release Closure 和 M7 入口

状态：待开始。

目标：

- usage、version、unsupported matrix、release baseline 和 normal repository samples 收口。
- full build、ctest、coverage、query/cache、sanitizer、stress 和 release gates green。
- 记录 M7 CFG-sensitive origin / loan / lifetime checker 入口。
- 继续后移 `dyn Trait`、region、isolation、async drop 和标准库重建。

## M7 预告：借用和生命周期安全

M7 不使用纯 lexical checker 作为长期地基。M7 目标：

```text
place / projection
-> read | mutate | consume
-> loan origin
-> CFG-sensitive conflict check
-> borrowed-return contract
-> lifetime surface
```

two-phase borrow、region surface、关系化 solver 和并发 capability 只在真实案例需要时进入后续 WP。

## 每轮验证门禁

```sh
tools/format_check.py $(git diff --name-only -- '*.cpp' '*.hpp') \
  $(git ls-files --others --exclude-standard -- '*.cpp' '*.hpp')
git diff --check
cmake --build cmake-build-release -j4
ctest --test-dir cmake-build-release --output-on-failure -j4
tools/check_coverage.sh -j4
make perf-stress-threshold
make query-sanitizer
```

大型收口：

```sh
make perf-release-threshold
```
