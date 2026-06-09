# 需求分析文档

## 当前分支目标

当前实现基线是 M13c Borrowed Composition-To-Supertrait IR / Backend Runtime。M6-WP2 到 M6-WP7 已落地资源分类、whole-local move
analysis、cleanup obligation lowering、drop-glue identity/planning、tooling projection 和 release closure；
M7 已完成 CFG-sensitive borrow/lifetime/resource hardening；M8 已完成 borrowed dyn trait frontend/sema、
checked vtable facts、IR/backend dynamic dispatch 和 hardening closure；M9 已完成 dyn ABI/tooling release
closure；M10 已完成 direct supertrait declaration、borrowed dyn-to-dyn upcast facts、`trait_object_upcast` IR、
`supertrait_vptr_metadata_v1` runtime projection、query/cache/tooling polish 和 release closure；M11a 已选择
principal-set borrowed dyn composition 作为后续 advanced dyn 主线，并固定 `principal_set_metadata_v1`、
`principal_set_identity_fact`、`composition_witness_set_fact`、`principal_method_namespace_fact`、
`associated_equality_merge_fact` 和 `composition_projection_fact` 的 design/query gate；M11b 已新增
`PrincipalSetCompositionFacts`、`principal_set_identity_fact()`、summary/dump/fingerprint 和 focused validation tests；
M11c 已新增 `dyn (A + B)` borrowed composition annotation/coercion 的 parser/AST/type/sema 实现；M11d 已新增
显式 composition-to-principal runtime projection、IR verifier、LLVM metadata 和 native execution coverage；M11e 已新增
`FunctionDynAbiFacts` composition runtime descriptors、lower-IR invalidation、IDE hover/semantic fact、verifier
negative matrix 和 release documentation closure；M12a 已新增无歧义 direct composition method dispatch；M12b 已补齐
receiver-access binding、associated equality direct dispatch、projection/ABI descriptor 去重、query/cache fingerprint
drift 和 broader negative matrix；M13a 已选择 borrowed composition-to-supertrait explicit projection 作为下一条
advanced dyn 主线，并新增 `m13a_dyn_advanced_design_gate_baseline` query gate；M13b 已新增
`dynproject[SourcePrincipal, TargetSupertrait](view)` 显式投影、`composition_to_supertrait`
projection fact、`supertrait_projections` summary/dump/fingerprint 和 focused sema diagnostics；M13c 已把该显式投影
lowering 为 `trait_object_composition_project` + `trait_object_upcast` runtime。
历史基线说明：当前实现基线是 M6 Resource And Access Semantics 这条旧 release baseline 仍作为资源语义
设计入口保留，但当前分支已经继续推进到 M13c。
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
  M13c 已支持 `dynproject[Child, Parent](view)` 这类 borrowed composition-to-supertrait 显式投影 runtime lowering。
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
- owning dyn、`Box<dyn Trait>`、allocator、trait-object Drop dispatch、composition-to-supertrait 隐式多步 direct
  dispatch、bare `dyn A + B` parser syntax、associated const、default associated type、generic associated type、
  specialization、minimal implementation annotation、完整 Rust-style lifetime surface、async drop 和标准库重建。

M1 的语言级 `move(...)` 和 `noncopy struct` 不再属于 M2 当前需求。M6 不复活这套 ad hoc surface，而是按
`Copy`、`Discard`、`NeedsDrop`、future `MustConsume` 四维模型重新推进资源语义。
