# 架构设计文档

## 组件

- `src/lex`：词法分析。
- `src/parse`：递归下降 parser。
- `src/syntax`：AST、token、模块路径和 dump。
- `src/sema`：名称解析、类型系统、泛型实例化、pattern exhaustiveness、值语义和控制流检查。
- `src/ir`：Aurex IR、lowering、验证和 pass pipeline。
- `src/backend/llvm`：LLVM IR 生成。
- `src/driver`：文件读取、模块加载、编译流水线和 clang 调用。
- `src/cli`：`aurexc` 命令行入口。

## 分支边界

当前架构基线是 M14 Borrowed Dyn View Path Inference / Dispatch Release。M2 已移除标准库层：

- 没有 `std/` 源树。
- driver 不查找 std root。
- module loader 不自动追加 std import path。
- native executable 不自动追加 support source。
- install 规则只安装 compiler。

这使语言核心变更能直接通过自包含样例验证，避免标准库加载、host support 和 M1 样例掩盖编译器本身的语义与性能问题。

## 当前架构方向

当前编译器架构已经进入 query-backed、borrow/resource-aware 且支持 borrowed dyn trait dispatch 的形态：

- `unsafe` 边界覆盖 raw pointer、unchecked string 和 bit-level cast。
- ADT enum、pattern matching、array、slice、string 和 function type 构成不依赖 std 的基础值和 ABI 表达。
- nominal static trait、显式 impl、`where` trait predicate、static trait method dispatch、associated type 和 trait
  default method body 已进入稳定 baseline。
- `&dyn Trait` / `&mut dyn Trait` borrowed erased view 已接入 frontend/sema、checked vtable facts、IR verifier、
  LLVM vtable global 和 indirect call；这是 borrowed-only dynamic dispatch，不是 owning object model。
- `trait Child: Parent` direct supertrait declaration、checked supertrait graph、borrowed dyn-to-dyn upcast facts、
  `TraitObjectUpcastCoercionKey` 和 `DynUpcastAbiDescriptor` 已接入 frontend/query/sema；`trait_object_upcast` IR、
  `supertrait_vptr_metadata_v1` vtable metadata 和 LLVM parent vtable projection 已接入 runtime。
- M11a 已把下一条 advanced dyn 主线选为 principal-set borrowed dyn composition；M11b 已把该模型落成
  `PrincipalSetCompositionFacts` query prototype，覆盖 principal-set identity、composition witness set、
  principal-qualified method namespace、associated equality merge、composition projection、validation、
  summary/dump/fingerprint 和 focused query tests。M11c 已接入用户可写 `dyn (A + B)` borrowed composition
  annotation/coercion 的 parser/AST/type/sema 子集；M11d 已接入 `trait_object_composition_pack` /
  `trait_object_composition_project` IR、`principal_set_metadata_v1` LLVM metadata global 和显式 projection
  runtime；M11e 已把 `FunctionDynAbiFacts::principal_sets` / `FunctionDynAbiFacts::composition_projections`、
  lower-IR query invalidation、IDE semantic fact/hover 和 verifier negative matrix 收口成 release baseline；M12a
  已支持唯一 principal method 的 direct composition dispatch，lowering 为 composition projection + ordinary
  `vtable_slot` dispatch；M12b 已固定 receiver-access binding、associated equality direct dispatch、projection/ABI
  descriptor 去重、query/cache fingerprint drift 和 broader negative matrix。M13a 已把下一条 advanced dyn 主线
  选为 borrowed composition-to-supertrait explicit projection，并固定 `m13a_dyn_advanced_design_gate_baseline`
  query gate；该主线组合已有 `principal_set_metadata_v1` 与 `supertrait_vptr_metadata_v1`，不新增 runtime
  metadata。M14 已在唯一 source-principal path 下打开 expected-type composition-to-supertrait projection 和 direct supertrait method dispatch；M13b 已把该主线的 frontend/query/sema check-only
  子集落成 `dynproject[SourcePrincipal, TargetSupertrait](view)`，记录
  `CompositionProjectionFact{kind=composition_to_supertrait}` 和 `supertrait_projections` query summary；M13c 已将其
  lowering 为 `trait_object_composition_project` + `trait_object_upcast`，复用已有 principal-set metadata 和
  supertrait vptr metadata；M13d 新增 `FunctionDynAbiFacts::composition_supertrait_chains`、lower-IR fingerprint
  invalidation、IDE semantic fact/hover 和 verifier negative matrix，固定完整 `composition_project -> upcast`
  runtime chain。
- owning dyn、`Box<dyn Trait>`、allocator、dynamic Drop dispatch、歧义多 principal composition-to-supertrait 自动选择、specialization、default associated type、associated const 和
  generic associated type 仍是后续独立设计流。

## M2.5 前端方向

M2.5 的架构主线是 stable-ID-driven query，而不是先堆一层 LSP 适配器：

- 第一批 [M2.5 Query Key 设计](m2.5-query-key-design.md) 已经进入默认增量缓存主路径，
  Stable Semantic Query Key、Session Fast Handle、CanonicalTypeKey、
  GenericInstanceKey 和 diagnostics query 边界已经定牢。
- 现有 sema 结果已经固定为显式 typed identity、显式诊断 kind 和稳定 fingerprint。
- file parse、module graph、item signature、function body、generic instance、
  diagnostics 已有第一批 query row/edge、replay 和 provider-skip profile 覆盖。
- lossless CST / GreenTree 保留 trivia，AST 继续作为语义层消费的 lowered 结构。
- CLI、JSON 和后续 IDE 统一消费结构化 diagnostics，不依赖 message 文本反推语义。
