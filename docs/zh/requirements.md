# 需求分析文档

## 当前分支目标

当前实现基线是 M15 Advanced Dyn Ownership / Const Generic Boundary Design Baseline。M6-WP2 到 M6-WP7 已落地资源分类、whole-local move
analysis、cleanup obligation lowering、drop-glue identity/planning、tooling projection 和 release closure；
M7 已完成 CFG-sensitive borrow/lifetime/resource hardening；M8 已完成 borrowed dyn trait frontend/sema、
checked vtable facts、IR/backend dynamic dispatch 和 hardening closure；M9 已完成 dyn ABI/tooling release
closure；M10 已完成 direct supertrait declaration、borrowed dyn-to-dyn upcast facts、`trait_object_upcast` IR、
`supertrait_vptr_metadata_v1` runtime projection、query/cache/tooling polish 和 release closure；M11/M12 已完成
principal-set borrowed dyn composition 的 design/query/frontend/sema、显式 projection runtime、direct composition
dispatch 和 release hardening；M13 已完成 borrowed composition-to-supertrait explicit projection 的
frontend/query/sema、IR/backend runtime 与 query/cache/tooling/verifier release closure；M14 已完成唯一
source-principal path 下的 expected-type projection 和 direct supertrait method dispatch，并记录
`BorrowedDynViewPathFact`。M15 在此基础上新增两个 design/query gate：`m15_dyn_advanced_design_gate_baseline()`
固定 owning dyn、dynamic Drop dispatch 和 allocator policy 的 future boundary；
`m15_const_generic_design_gate_baseline()` 固定 typed scalar const parameter、canonical const value、generic
instance const arg key 和 `[N]T` array length 集成路线。M15 不实现标准库、不实现 owning dyn runtime、
不实现 `Box<dyn Trait>`、不生成 dynamic Drop dispatch，也不打开用户可写 const generic 语法。
历史基线说明：M6 Resource And Access Semantics 这条旧 release baseline 仍作为资源语义设计入口保留，
但当前分支已经继续推进到 M15。
较早的 M2 `language-core-no-std` 阶段用于隔离语言核心验证：

- 编译器必须能在没有标准库源树的情况下构建、安装和运行。
- import 只能来自导入者目录和显式 `-I`。
- native 输出不能隐式链接任何 std support 源文件。
- 样例和测试必须能区分语言特性耗时与外部脚本/标准库加载耗时。
- 语言核心样例应自包含；`Result` / `Option` 等用于 `?` 验证时在样例内定义。

## 保留能力

- 手写 lexer/parser。
- 模块、import、可见性、re-export。
- 基础类型、struct、ADT-style enum、显式 C-like/repr enum、generic struct/function/enum/type alias、owner generic impl 和 `where` capability / trait predicate。
- nominal static trait、显式 `impl Trait for Type`、static trait method dispatch、第一版 associated type model 和
  trait default method body。
- borrowed dyn trait view：`&dyn Trait` / `&mut dyn Trait`、`dyn Trait[Assoc = Type]`、checked vtable witness、
  IR/backend indirect dispatch。
- direct supertrait declaration 和 borrowed dyn supertrait upcast runtime：`trait Child: Parent`、checked
  `TraitSupertraitEdgeFact`、`TraitObjectUpcastCoercionKey`、`DynUpcastAbiDescriptor`、`trait_object_upcast` IR、
  `supertrait_vptr_metadata_v1` vtable metadata、LLVM parent vtable projection、query/cache invalidation 和 IDE
  hover/tooling projection。
- M11a/M11b advanced dyn design/query facts：`m11a_dyn_advanced_design_gate_baseline`、
  `principal_set_metadata_v1`、`PrincipalSetCompositionFacts` 和 principal-set composition required facts 已固定，
  M11c 已支持 `dyn (A + B)` borrowed composition annotation/coercion，M11d 已支持显式 runtime projection，M11e
  已把 runtime projection facts 接入 query/cache/tooling/verifier release closure，M12a/M12b 已支持并收口唯一
  principal method direct composition dispatch，M13a 已固定 `borrowed_composition_supertrait_projection` 设计门禁，
  M13c/M13d 已支持并收口 `dynproject[Child, Parent](view)` 这类 borrowed composition-to-supertrait 显式投影
  runtime lowering；M14 已支持唯一 path 的 `let parent: &dyn Parent = view;` 和 `view.parent()`。
- M15 design/query facts：`m15_dyn_advanced_design_gate_baseline()` 固定 owning dyn / dynamic Drop /
  allocator policy 的后续边界；`m15_const_generic_design_gate_baseline()` 固定 const generic 后续实现必须走
  typed scalar const param、canonical const value key、generic instance const arg key 和 `[N]T` array length
  integration，不允许用 display text 或 `TypeHandle` placeholder 偷代 const argument identity。
- pattern matching、guard、or-pattern。
- `if` expression、block expression、`while`、`for`、`break`、`continue`。
- `defer` 和 `?`。
- compiler-owned `Copy`、内部 `Discard` / `NeedsDrop` resource summary、whole-local move diagnostics、
  lexical cleanup lowering、stable drop-glue identity、IDE resource hover 和 `aurex-lsp` stdio 入口。
- 普通值语义；数组和含数组类型不能作为函数 by-value 参数/返回、赋值目标或 enum payload 的限制暂时保留。
- Aurex IR、pass pipeline、LLVM IR、clang native 输出。

## 暂缓能力

- 标准库 API、容器、文件、目录、进程、console。
- M1 frontend/build-tool 样例。
- std host support 和安装后 std 查找。
- owning dyn、`Box<dyn Trait>`、allocator、trait-object Drop dispatch、歧义 composition-to-supertrait 自动选择、
  bare `dyn A + B` parser syntax、用户可写 const generic 语法、generic const arithmetic、const where
  predicates、associated const、default associated type、generic associated type、specialization、minimal
  implementation annotation、完整 Rust-style lifetime surface、async drop 和标准库重建。

M1 的语言级 `move(...)` 和 `noncopy struct` 不再属于 M2 当前需求。M6 不复活这套 ad hoc surface，而是按
`Copy`、`Discard`、`NeedsDrop`、future `MustConsume` 四维模型重新推进资源语义。
