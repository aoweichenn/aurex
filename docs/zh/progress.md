# 当前进度文档

版本：0.1.5
阶段：M10d Supertrait Hardening / Release Closure

## 总体状态

2026-06-08：M10d Supertrait Hardening / Release Closure 已完成。M10 现在收口为完整 borrowed dyn supertrait
upcasting release baseline：M10a 固定设计，M10b 完成 frontend/query/sema facts，M10c 完成 IR/backend runtime，
M10d 完成 query/cache/tooling polish、negative sample matrix、文档收口、coverage gate 和 M10c 实际代码量偏差分析。
新增收口文档见 [Aurex M10 Supertrait Upcasting Release Baseline](m10-release-baseline.md)。

M10d 仍保持 compiler/runtime core 范围，不实现标准库、不实现 `Box<dyn Trait>`、不实现 owning dyn、不实现
dynamic Drop dispatch、不实现 allocator policy、不实现 multi trait composition，也不新增 trait-object Drop metadata。
当前稳定能力是 borrowed dyn-to-dyn upcast：`&dyn Child -> &dyn Parent`、
`&mut dyn Child -> &mut dyn Parent` 和 `&mut dyn Child -> &dyn Parent`；upcast 是 coercion，不是普通子类型，
不创建 ownership、不复制对象、不延长 origin、不放宽 loan、不把 shared borrow 升级成 mutable。

M10d 已补齐 tooling/query 收口：`FunctionDynAbiFacts::upcasts` 现在会作为 dyn ABI surface 触发 lower-IR
query/semantic fact；IDE hover 展示 `metadata=supertrait_vptr_metadata_v1`、`upcasts=N`、首条 upcast source/target
和 borrow kind；summary/dump/fingerprint/lower-IR invalidation 已覆盖 upcast edge 和 borrow 变化。常规 negative
sample suite 已覆盖非 supertrait target、shared-to-mut upgrade 和 missing parent evidence；IR/verifier focused tests
覆盖 layout/edge mismatch。

M10c 的实际 diffstat 是 `37 files changed, 1316 insertions(+), 255 deletions(-)`，低于 1,600-2,800 行预估。
主要原因是 M10b 已提前铺好 checked/query DTO，M8/M9 已有 borrowed dyn ABI/tooling 基线，LLVM lowering 复用
现有 fat-view/indirect-call 路径，且标准库、owning dyn、Drop dispatch、allocator 和 multi trait composition 均未进入
M10c/M10d。

M10 已结束，下一步应进入 **M11 Advanced Dyn Design Baseline**：从 M9c gate 的剩余 advanced dyn 候选中选择
后续主线，先设计 policy/schema 和 query gate，不直接实现标准库、owning dyn 或 dynamic Drop dispatch。

2026-06-08：M10c Supertrait IR / Backend Runtime Implementation 已完成。M10c 继续保持 compiler/runtime
core 范围，不实现标准库、不实现 `Box<dyn Trait>`、不实现 owning dyn、不实现 dynamic Drop dispatch、不实现
allocator policy、不实现 multi trait composition，也不新增 trait-object Drop metadata。当前新增能力是把 M10b
已经验证的 borrowed dyn-to-dyn upcast facts 贯通到 IR、verifier、LLVM backend 和 native execution。

Sema 现在可以把 `dyn Child` receiver 上 inherited parent method call 绑定为 runtime-ready parent dispatch：
`child.parent()` 会设置 `dispatch_receiver_type=dyn Parent`，记录 `TraitObjectUpcastCoercionFact`，并在后续
concrete-to-child coercion 出现时回填 source/target vtable layout。Checked facts 仍保留 upcast 是 coercion、
不是普通子类型的边界；它不创建 ownership、不复制对象、不延长 origin、不放宽 loan、不把 shared borrow 升级成
mutable。

IR 侧新增 `trait_object_upcast` value，`TraitObjectVTableLayout` 新增 `supertrait_edges`。Lowering 会从
checked upcast fact 生成 runtime upcast，并保证每个 concrete `dyn Child` vtable 都有同序 parent edge；同一个
`score(child: &dyn Child)` 在 File/Socket 等不同 concrete impl 上会 late bind 到各自 parent vtable。IR dump、
fingerprint、pass pipeline、`FunctionDynAbiFacts` 和 verifier 均已纳入 upcast / supertrait edge invariants。

LLVM backend 现在用 `supertrait_vptr_metadata_v1` 表达带 parent vtable pointer metadata 的 vtable global：
global shape 为 `{ [methods x ptr], [supertraits x ptr] }`。`trait_object_upcast` lowering 复用 data pointer，
从 source vtable 的 supertrait table 加载 target parent vtable pointer，再构造 target `{data*, vtable*}`
borrowed dyn view。Native execution 已覆盖 inherited parent dispatch、多 concrete child vtable 和 parent vtable
projection。

M10c 的下一步已由 M10d Supertrait Hardening / Release Closure 完成。标准库、owning dyn、`Box<dyn Trait>`、
allocator、dynamic Drop dispatch 和 multi trait composition 仍是独立后续阶段，不进入 M10。

2026-06-08：M10b Supertrait Frontend / Query / Sema Implementation 已完成。M10b 仍然是 compiler-only /
check-only 子集，不实现标准库、不实现 `Box<dyn Trait>`、不实现 owning dyn、不实现 dynamic Drop dispatch、不实现
multi trait composition，也不做 LLVM runtime upcast lowering。当前新增能力是把 supertrait upcasting 的
frontend/query/sema 事实链落到代码，而不是把运行时 metadata 偷偷塞进 `borrowed_methods_only_v1`。

M10b 现在可以解析和 dump direct supertrait clause：`trait Child: Parent, Other where ... { ... }`。AST 新增
`TraitSupertraitDecl`，trait item 保存 `trait_supertraits`，parser 对缺失 parent name、缺失 separator 等语法错误有
focused diagnostics。Checked module 新增 `TraitSupertraitInfo`、`TraitSupertraitEdgeFact` 和
`TraitObjectUpcastCoercionFact`，copy/move/swap、stable fingerprint、checked dump、text rebind 和
`TypeCheckBodyAuthority` 都已纳入 supertrait edge / upcast counts。

Sema 现在会解析 direct/transitive supertrait graph，拒绝 duplicate direct parent、direct/indirect cycle 和 public trait
暴露 private supertrait；`impl Child for T` 会要求可证明的 parent evidence，泛型 parent args 会按当前 impl 实参替换。
borrowed dyn-to-dyn coercion 作为 coercion site 记录：允许 `&dyn Child -> &dyn Parent`、
`&mut dyn Child -> &mut dyn Parent` 和 `&mut dyn Child -> &dyn Parent`，拒绝 shared-to-mut、非 supertrait target
和泛型 target mismatch。该 upcast 不创建 ownership、不延长 origin、不放宽 loan、不把 shared borrow 升级成 mutable。

Query / ABI facts 侧新增 `TraitObjectMetadataPolicyKey::supertrait_vptr_metadata_v1`、
`TraitObjectUpcastCoercionKey`、`DynMetadataPolicy::supertrait_vptr_metadata_v1` 和 `DynUpcastAbiDescriptor`。
`FunctionDynAbiFacts` 可以投影 upcast descriptors、summary、fingerprint 和 dump。M10b 的 query key validation
只验证 source/target object、origin、edge fingerprint、borrow kind 和 policy 形状；target 是 source supertrait 的证明来自
sema checked fact。`trait_object_upcast` IR、`VTableSupertraitEdgeDescriptor` 等价 IR fact、LLVM
`supertrait_vptr_metadata_v1` vtable global 和 runtime projection 已由 M10c 完成。

2026-06-08：M10a Supertrait Upcasting Design Baseline 已完成。M10 已从 `m10` 分支开启，并从 M9c
`DynAdvancedDesignGate` 的 advanced dyn 候选中选择 supertrait upcasting 作为第一条后续主线。新增
[Aurex M10 Supertrait Upcasting 设计基线](m10-supertrait-upcasting-design.md)，基于当前代码事实固定
borrowed dyn-to-dyn coercion：`&dyn Child -> &dyn Parent` 和 `&mut dyn Child -> &mut dyn Parent`。

M10a 的核心设计结论是：upcast 是 coercion，**不是普通子类型**；它保持 origin-bound erased view，data pointer
不变，只替换或投影 parent vtable pointer。当前 `borrowed_view_v1` 仍可作为 borrowed fat view ABI，当前
`borrowed_methods_only_v1` 不能承载 supertrait edge metadata，必须新增 `supertrait_vptr_metadata_v1`。M10a
固定后续实现需要的 `TraitSupertraitEdgeFact`、`TraitObjectUpcastCoercionKey`、
`DynUpcastAbiDescriptor` 和 `VTableSupertraitEdgeDescriptor`，并把 parser/sema/query/IR/backend/diagnostics/
tests 的分层计划和代码量预估写入文档。

M10a 不新增 parser/sema/IR lowering/LLVM backend runtime 或标准库代码。本阶段继续不实现标准库、不实现
`Box<dyn Trait>`、不实现 owning dyn、不实现 dynamic Drop dispatch、不实现 multi trait composition，也不把 runtime
feature 藏进 `borrowed_view_v1` / `borrowed_methods_only_v1`。M10a 的下一步已由 M10b frontend/query/sema
承接，M10b 的下一步也已由 M10c IR/backend runtime 承接；标准库仍不属于 M10。

2026-06-08：M9 Dyn ABI / Tooling release closure 已完成。M9a-M9c 已收成
[Aurex M9 Dyn ABI / Tooling Release Baseline](m9-release-baseline.md)：M9a 固定 facts-first dyn ABI / tooling
设计，M9b 实现 `FunctionDynAbiFacts` borrowed dyn ABI facts，M9c 实现 `DynAdvancedDesignGate` advanced dyn
设计准入 gate，M9d 完成 release 文档、状态入口、文档测试和 release gate 收口。

M9 release closure 没有新增 parser/sema/IR lowering/LLVM backend runtime 或标准库代码。当前唯一 ABI policy 仍是
`borrowed_view_v1`，当前唯一 metadata policy 仍是 `borrowed_methods_only_v1`。advanced dyn 方向必须通过后续
独立阶段新增 policy/schema，不能复用当前 borrowed policy 偷偷扩展。M9 结束后，下一步建议进入 M10 planning /
post-M9 advanced dyn policy selection，而不是继续把标准库或 runtime feature 塞进 M9。

2026-06-08：M9c Advanced Dyn Design Gate Baseline 已完成。本阶段没有实现标准库、`Box<dyn Trait>`、owning
dyn runtime、allocator API、dynamic Drop dispatch runtime、supertrait upcasting runtime 或多 trait object
composition runtime；实现边界是 compiler/query 侧的 advanced dyn 设计准入 gate。query 层新增
`DynAdvancedDesignGate` / `DynAdvancedDesignCandidate` DTO，覆盖 `supertrait_upcasting`、`owning_dyn`、
`dynamic_drop_dispatch`、`allocator_policy` 和 `multi_trait_composition` 五个候选方向，并提供 validation、
stable fingerprint、summary 和 dump。

M9c gate 的核心结论是：advanced dyn 不能复用当前 borrowed baseline 的 `borrowed_view_v1` /
`borrowed_methods_only_v1` 偷偷扩展。supertrait upcasting 和 multi trait composition 只能进入后续
metadata-policy design gate；owning dyn 和 allocator policy 被标准库阶段阻塞；dynamic Drop dispatch 被 runtime
阶段阻塞。每个候选都记录 required policy、blockers、required facts、borrow/drop/resource/tooling/cache impact
和 `standard_library_runtime_not_in_m9c` non-goal。新增 focused query tests 固定五个候选、invalid enum fallback、
policy drift、stage drift、fingerprint drift 和 dump 关键文案。

2026-06-07：M9b Dyn ABI / Tooling implementation baseline 已完成。M8 release closure 已完成，当前主线不再
给 M8 追加语言语义，而是把 M8 已经能运行的 borrowed dyn runtime dispatch 固化成可跨 backend、query/cache、
IDE/tooling 和后续 advanced dyn 设计复用的 facts-first ABI 基线。M9a 新增
[Aurex M9 Dyn ABI / Tooling 设计基线](m9-dyn-abi-tooling-design.md)，M9b 则把该设计落到代码：query 层新增
library-independent `FunctionDynAbiFacts` DTO，覆盖 object descriptor、vtable descriptor、slot descriptor、
coercion descriptor 和 dispatch descriptor；DTO 提供 validation、stable fingerprint、summary 和 dump。

M9b 明确不实现标准库、`Box<dyn Trait>`、owning dyn、allocator、dynamic Drop dispatch、supertrait upcasting
或多 trait object composition；也不改变 M8 的 `&dyn Trait` / `&mut dyn Trait` 语义。当前 M9 只承认
`abi=borrowed_view_v1` 和 `metadata=borrowed_methods_only_v1`。实现层已经把 `TraitObjectTypeKey`、
`VTableLayoutKey`、`TraitObjectCoercionKey`、checked vtable facts、IR
`trait_object_pack/data/vtable/vtable_slot` 和 LLVM `{data*, vtable*}` borrowed view lowering 投影成 ABI/tooling
facts，而不是从 IR dump 或 backend 文本反推。

M9b 同步完成 checked adapter、IR adapter、lower-IR query/cache invalidation 和 IDE tooling projection：
checked-module adapter 能导出 object/vtable/coercion/dispatch facts；IR adapter 会按函数 value closure 只投影当前
函数实际使用的 dyn ABI layout 和 `vtable_slot` dispatch；lower-function-IR result fingerprint 现在混入 cleanup
facts 与 dyn ABI facts；IDE semantic facts / hover 可展示 `abi=borrowed_view_v1`、
`metadata=borrowed_methods_only_v1` 和 `dispatch=vtable_slot slot=N`。新增 query/IR/sema/tooling focused tests
覆盖 DTO validation、fingerprint/dump、adapter 投影、IDE hover 和 lower-IR invalidation。

2026-06-07：M8 release closure 已完成。M8a-M8e borrowed dyn trait runtime dispatch closure 已完成，M8
follow-up sample / release polish 主项也已完成。M8 主线从最新 M7 基线开到
`m8` 分支后，先完成 M8a Borrowed Erased Trait View query foundation：dyn trait / erased view 调研设计
基线已固定，query 层错误地基同步改掉。无语义形状的
`CanonicalTypeKind::trait_object` 占位已从代码中移除，stable key decoder 和 query tests 不再承认 0-child
trait object canonical key。

M8a 新增 `TraitObjectTypeKey`、`VTableLayoutKey` 和 `TraitObjectCoercionKey` 三类结构化 query identity，
分别表达 borrowed erased view 类型、checked vtable witness 和 borrow-to-dyn coercion。decoder 会验证 schema、
policy、principal trait、associated type member、嵌套 canonical type 和 key layout；query tests 覆盖稳定
serialization/hash/debug、identity decode、associated equality 归一化和 malformed layout rejection。

M8b/M8c frontend closure 已完成：lexer/parser/AST 接受 `dyn Trait`、qualified dyn trait、trait args 和
associated equality；sema 产生 `TypeKind::trait_object`，并只允许 bare `dyn Trait` 作为 reference pointee 等
显式 allowed context。object-callability 现在会诊断缺少 self receiver、非 `&Self` / `&mut Self` receiver、
未约束 associated type、未知/重复 associated equality，以及不允许出现在可调用签名中的 unconstrained `Self`。
显式约束的 `Self.Assoc` 会被 `dyn Trait[Assoc = Type]` 替换进 slot 签名。

M8c 同步完成 borrowed dyn coercion 和 checked facts：`&T -> &dyn Trait`、`&mut T -> &mut dyn Trait` 会检查可见
nominal impl 与 associated equality，一致时记录 `TraitObjectCoercionFact`、`VTableLayoutFact`、
`TraitObjectMethodSlotFact` 和 `TraitObjectCallabilityFact`；dyn receiver method call 绑定为
`TraitMethodDispatchKind::vtable_slot`，checked dump 会展示 slot ordinal、receiver access、vtable layout 和
coercion fingerprint。CheckedModule copy/move/swap、stable fingerprint、TypeCheckBodyAuthority 和 query key
路径均已纳入新 facts。

M8d/M8e runtime dispatch 和 hardening 也已完成：checked vtable method slot facts 会绑定 impl override 或
trait default instance function；IR 新增 trait-object pack/data/vtable/slot value kind 和 vtable layout；
verifier 检查 layout 唯一性、slot 范围、function target、erased receiver ABI 和 fat-view type invariant；
LLVM backend 将 `&dyn Trait` / `&mut dyn Trait` lowering 为 `{data*, vtable*}`，生成 internal vtable global，
slot load 后通过已有 indirect call path 派发。native execution tests 覆盖 shared dyn dispatch、mutable dyn
receiver 写回、default method slot 和 `dyn Trait[Assoc = Type]` associated equality dispatch。

M8 follow-up 的 sample / release polish 主项已完成：`trait_dyn_borrowed_dispatch.ax` 进入 positive runtime
sample suite，覆盖 shared dyn、mutable dyn、default method slot 和 associated equality；缺失 associated
equality 与缺失 nominal impl coercion 也进入 negative sample suite，确保用户层诊断不只停留在白盒测试里。

M8 不照抄 Rust / Swift / Go / C++ 的任一套对象模型。Aurex 当前选择 **origin-bound erased view**：
第一版只做 borrowed dyn view，当前可用 surface 是 `&dyn Trait` / `&mut dyn Trait`，复用 M7 origin / loan /
lifetime facts，以 checked vtable witness 描述动态派发。本阶段继续不实现标准库，不实现 `Box<dyn Trait>`、
allocator、owning existential container、dynamic Drop dispatch、supertrait upcasting 或多 trait object
composition。M8 当前已完成 M8a-M8e，M9 也已完成 dyn ABI / tooling release closure；后续应进入 M10 planning /
post-M9 advanced dyn policy selection，而不是继续把标准库或 owning dyn 塞进已收口阶段。

M8 封口结论：`m8` 分支只承载 borrowed dyn runtime dispatch 的 release baseline，不再增加语言语义。M9 已在
`m9` 分支完成 dyn ABI / tooling release baseline：library-independent dyn ABI DTO、tooling/query 投影、
cross-module invalidation、verifier/backend negative matrix 和 advanced dyn design gate 已收口；标准库、
owning dyn、allocator 和 dynamic Drop dispatch 仍不属于 M9 release 实现范围。

2026-06-05：M7d-F tuple / index place-state closure 已完成 compiler-only 子集。本阶段继续不实现任何标准库。
parser 现在接受匿名 tuple 数字字段访问：`pair.0` 和 `pair . 0` 都会进入普通 field expression；`pair.0f32`
仍按 suffixed float 边界处理。sema 会把合法范围内的 tuple 数字字段解析为对应元素类型，并把 tuple 元素
作为可写 place 参与 assignment；非数字字段报 `tuple field access requires a numeric field`，越界数字字段报
`tuple field index is out of range`。

M7d-F 同时把 tuple element projection 接入 body-flow、borrow checker、place-state、dropck、move analysis
和 IR lowering：不同已知 tuple 元素在 borrow conflict matrix 中可被视为 disjoint；本地 owned tuple 元素可以
partial move/reinit；cleanup/dropck facts 会沿 tuple 元素投影求类型；lowerer 会为 droppable tuple 元素建立
元素级 cleanup/drop flag，move-out 后关闭对应 flag、reinit 后重新打开，scope cleanup 只 drop 仍 initialized
的 tuple 元素。当前仍保守的是 indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern
payload、non-`Copy` `?` payload transfer、borrowed/reference 字段 resource overwrite、replace/take/swap
primitive、generic/opaque Drop ABI 和标准库拥有型资源封装。

2026-06-05：M7d-E aggregate rollback codegen closure 已完成实现收口。本阶段只补编译器 lowering，
不实现任何标准库。lowerer 现在会在函数体内对含 droppable 元素的 struct literal、tuple literal、array literal
和多 payload enum synthetic record payload 走 staged aggregate lowering：先为 aggregate 分配临时 slot，逐元素
求值并写入；每个 droppable 元素只有在求值成功且写入后才注册 rollback drop flag。后续元素表达式如果通过
`return` 等路径提前终止，已有 cleanup stack 会在 early-exit cleanup 中发出 `drop_if` marker，并通过 M7d-D
的 runtime drop lowering 调用静态可解析 custom destructor，从而只清理已经初始化成功的元素。

M7d-E 保持保守边界：global/constant initializer、无 droppable 元素的 scalar aggregate 和普通 scalar aggregate
仍保持原 lightweight `ValueKind::aggregate` 路径；本阶段不解锁标准库级拥有型资源封装、不支持用户可写 `Drop`
bound、generic Drop impl、trait-object Drop dispatch、generic/opaque Drop ABI、async/unwind-aware drop、panic
cleanup ABI、non-`Copy` `?` payload transfer 或 indexed move-out；tuple `.0` / `.1` source surface 与本地
tuple 元素 partial move 已由 M7d-F 补上。

2026-06-05：M7d-D RAII runtime lowering 与 execution closure 已完成实现收口。lowerer 现在会从 checked
`DestructorInfo` / `FunctionSignature::c_name` 解析 custom destructor 的 IR `FunctionId`，在 cleanup exit、
overwrite、early-exit 和 drop flag guarded path 上生成普通 direct `call`；LLVM backend 复用现有 call emission
发出真实 destructor call。`drop` / `drop_if` 仍保留为 target-independent cleanup marker，marker 自身在 LLVM
backend 仍是 no-op；真实运行时副作用来自 lowering 额外生成的 call。

M7d-D 同时修正了带 custom destructor 且含 droppable 字段的 struct local：根 custom destructor 使用独立根
drop flag，并在字段 cleanup 之前运行；字段级 drop flag 继续支持本地 field partial move/reinit。`self: deinit T`
参数不会再注册普通 lexical cleanup，避免 destructor body 退出时递归/重复 drop 自己。当前 runtime lowering 覆盖
静态可解析 custom destructor、struct/tuple/array 已知子对象和 active enum payload 的 custom destructor 展开；
generic/associated/opaque cleanup、`Drop` bound、generic Drop impl、trait-object Drop dispatch、async/unwind-aware
drop、panic cleanup ABI 和标准库拥有型资源封装继续后移。

2026-06-05：M7d-C RAII user surface 与 release closure 已完成实现收口。当前 parser/AST 接受
`fn drop(self: deinit T) -> void` 的 contextual `deinit` 参数修饰符；sema 将 `Drop` 作为 compiler-owned
reserved destructor surface 处理，允许窄 `impl Drop for T`，并拒绝 `trait Drop`、qualified/type-arg
Drop surface、generic Drop impl、associated type、额外方法、borrow contract、unsafe/extern/export/variadic
Drop method、非 `void` 返回、非 `self: deinit T` receiver 和非 named struct/enum/opaque struct 目标。

合法析构器会进入 `CheckedModule::destructors`，对应 `FunctionSignature` 标记为 `is_destructor`；checked dump、
clone/copy、stable fingerprint、resource classifier、drop-glue plan、dropck facts、`TypeCheckBodyAuthority`、
IR verifier 和 IDE hover 均已接入。带 custom destructor 的类型按 conservative owned resource 处理；
drop-glue 会先生成 `custom_destructor` step，再展开结构字段或泛型/opaque cleanup；dropck action 使用
destructor info fingerprint，drop facts 记录 destructor function；IDE hover 展示 `destructor=custom`。
M7d-C 当时只声明 semantic/checking/tooling closure；M7d-D 已补上静态 custom destructor 的 runtime direct-call
lowering。用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、async/unwind-aware drop 和标准库
拥有型资源封装继续后移。

2026-06-05：M7d-B struct field place-state 子集已完成实现收口。当前代码允许本地 owned struct field
partial move 和 field reinit，例如 `let moved: T = current.left; current.left = replacement;`；body-flow 会把 field
assignment 记为 `reinit`，并为 droppable struct fields 生成 projected `cleanup_storage`。generic template
body-flow 会读取 generic side table 的 expr/local 类型，因此 `Box[T]` 字段 cleanup 能看到字段类型。IR lowering
为 droppable struct fields 建立字段级 drop flag，在 field move-out 后关闭对应 flag，在 field reinit 后重新打开，
cleanup 只 drop 仍 initialized 的字段。place-state 还修正了 owned generic return summary 的解释：`id[T](value) -> T`
这类返回 owned `T` 的 summary 仍保留给 borrow/lifetime checker，但不会在 place-state 中被误当成对 moved 实参的借用。

当前 M7d-B 后续已经由 M7d-F 补上 tuple `.0` / `.1` source surface、本地 tuple 元素 partial move/reinit 和
tuple 元素级 cleanup/drop flag。仍保守拒绝的是 indexed move-out、array/slice/index 精确 disjoint proof、
consuming pattern payload、non-`Copy` `?` payload transfer；通过 borrowed/reference base 的 resource field
overwrite 仍拒绝；`replace` / `take` / `swap` 还没有 compiler-known primitive 或标准库 intrinsic。

2026-06-04：M7c-C storage escape 事实链与性能收口完成。`FunctionBorrowSummary` 新增
`storage_escapes`，summary builder 会把非 name storage assignment 中来自 local / temporary /
parameter-storage origin 的 borrowed value 记录为稳定事实；lifetime collector 会把这些 storage escape 映射为
`local_escape` / `unknown_escape` violation，并由 lifetime enforcer 发出主诊断。`TypeCheckBodyAuthority`、
checked dump、query fingerprint、IDE semantic fact 和 hover 现在都暴露 `storage_escapes`、`local_escapes` 和
`unknown_escapes`。旧 `BorrowEscapeAnalyzer` 不再负责 summary 已覆盖的 return/storage 主路径，只在 summary
缺失或无 storage escape 且 body 存在候选 non-name assignment 时作为窄 parity guard 运行，避免重复诊断。

同日完成 M7c 热点 O(n²) 收口：borrow summary origin 去重改为 hash lookup；lifetime duplicate facts /
violation 去重改为 hash index；lifetime outlives 从 dense transitive matrix 改为 sparse successor + per-region
reachability cache；body loan checker 增加 carrier definition/result binding index、issued-action loan index、
type-contains-reference cache、reborrow parent 最新 carrier index 和 two-phase activation queue，移除 storage
escape 压测中的全 body / 全 loan 反复扫描。实际性能使用 `aurexc --profile-output`、`cmake -E time` wall-time
spot-check 和 `gprof` 在本机 `build/full-llvm-fedora` / `build/gprof-m7c` 上验证：当前 storage escape
`sema.analyze` 3-run median 500/1000/2000/4000 条分别为 50.701 / 100.202 / 204.232 / 419.745 ms；
优化前同类 500/1000/2000 条为
323.151 / 1007.543 / 4821.837 ms。更新后的 `gprof` 2000 条报告中，旧热点
`BodyLoanSolver::expr_result_contains_loan` 不再出现在热点调用列表，solver 内 `type_contains_reference` 为 4000
次，随输入规模线性增长。代表性 6-case 3-run median：sample negative sema 1.469 ms，summary_1000
79.936 ms，summary_2000 159.321 ms，storage_escape_1000 100.569 ms，plain_1000 32.087 ms，plain_2000
62.695 ms。

2026-06-03：M7c-A / M7c-B 已完成实现收口。parser/AST/type system 的 contextual `origin` 参数、
`&[origin] T` / `&mut[origin] T` 和 origin union 已进入 checked facts；`CheckedModule` 现在保存
`FunctionLifetimeFacts`、`TypeLifetimeInfo`、`GenericLifetimePredicate`，checked dump、stable fingerprint、
`TypeCheckBodyAuthority` 和 IDE `lifetime_facts` detail 已混入 lifetime region、type-outlives、live-range、
type/generic predicate 和 diagnostics 状态。lifetime analyzer 保持 collector / solver / enforcer 分层：collector
从 signature、显式 reference origin、`@borrow(...)` / inferred `BorrowSummary` 收集 region/type facts；solver
用确定性 outlives matrix fixed point 和 body-flow point live-range facts 产出 stable solved facts；enforcer 负责
elision ambiguity、return-origin subset、type-outlives 和 local/temporary escape 主诊断。`BorrowSummaryBuilder` 现在会
追踪 `strraw` raw pointer alias 的本地来源，raw-derived local return 由 lifetime checker 诊断为
`borrowed local storage cannot escape the function`；unsafe raw pointer parameter helper 仍保守记录 unknown return
fact，不把所有 unknown proof 当成错误。旧 `BorrowEscapeAnalyzer` 已从 return escape 主路径降级为 storage-only
parity guard，继续覆盖 assignment into escaping struct field 等尚未迁移的存储逃逸矩阵；M7c-C 再处理 public /
prototype / extern / trait lifetime release policy 与旧 analyzer cleanup。

2026-06-03：M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线已固定，记录在
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。
新基线明确不照抄 Rust lifetime surface，而是采用 `@borrow(return = [...])` 作为函数边界主 contract，并选择
`&[origin] T` / `&mut[origin] T` 作为显式 origin-qualified reference type；该语法沿用 Aurex 现有 `&T` /
`&mut T`，避免 Rust apostrophe lifetime、避免新增 `ref` 关键字，并与 `Name[T]` 泛型、`[]const T` slice、
`[16]T` 数组保持无歧义。该语法只在 type context 中解析，不占用未来 lambda/closure 的表达式语法空间；完整
closure/lambda capture 后续独立设计，但 M7c facts 预留 `ClosureCaptureFact` / `ClosureEnvironmentFact`。
设计同时把 M7c/M7d 拆为 M7c-A/B/C lifetime facts + region solver + old analyzer replacement，以及 M7d-A/B/C
dropck + place-level resource state + RAII surface。实现架构新增硬约束：
`src/sema/internal/` 只能作为 private implementation root，不再直接新增文件；M7c/M7d 新代码必须按
`borrow/`、`lifetime/`、`dropck/`、`place/`、`diagnostics/`、`pipeline/` 等职责拆子目录，其他 compiler stage
同样执行。全局 `compiler-engineering` 和 `cpp-project-standards` skill 已同步该目录解耦规则。

2026-06-03：M7b WP1-WP7 已完成实现收口。`FunctionBorrowContract` 进入 `CheckedModule`、checked dump、
query/cache 和 IDE facts；parser/AST/sema 支持函数声明前装饰器式 `@borrow(return = [...])`，并与 trait
requirement / impl method contract 做 subset matching。summary-vs-contract enforcement 已拒绝 declared contract
外的 local/temporary/unknown return borrow source。`BodyLoan` 现在记录 reborrow `parent_loan`，child loan 的有效
place 会归一到 parent 底层 place，child 活跃期间对 mutable parent carrier 的 read/write/reborrow 会报
`reborrow_parent_use`，known-disjoint field projection 仍可通过。method / trait method call binding 现在记录
`receiver_access`、`receiver_auto_borrow` 和 `receiver_two_phase_eligible`；body flow 只为语义确认的 mutable
reference receiver auto-borrow 生成 two-phase reserve/activate action。two-phase reservation 期间允许 shared read，
拒绝 write/reinit/move/drop/cleanup、mutable borrow 和 nested mutable receiver reservation；activation 点会与活跃
loan 做完整 mutable conflict check。`TypeCheckBodyAuthority` 现在混入 borrow contract fingerprint、
body loan fingerprint、reborrow/two-phase counts 和 diagnostics-emitted 状态位；IDE `body_loan_check` detail 展示
`loans/reborrows/two_phase/conflicts`。`BorrowEscapeAnalyzer` 仍保留为旧 borrowed-local escape 诊断路径，移除需等
后续更大 parity matrix 专项。

2026-06-02：M7b 设计基线已固定。M7b 不继续往 M7a 混入新表面，而是把 M7a 的
`BorrowSummary` / `BodyLoanCheckResult` 事实提升为函数边界 `FunctionBorrowContract`，选择窄 surface
`@borrow(return = [param, self])`，并把 reborrow parent/child loan、method receiver access、receiver auto-borrow
two-phase reservation/activation、trait/generic/extern borrowed-return contract 和 `BorrowEscapeAnalyzer` parity
replacement 作为下一实现包。完整设计见
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md)，路线图见
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图](m7b-roadmap.md)。M7b 仍不做 full
Rust-style lifetime generics、full Polonius Datalog、raw pointer alias safe proof、partial move / replace /
take / swap、`dyn Trait`、async drop 或 generator borrow。

2026-06-02：M7a WP6/WP7 已完成实现收口。`TypeCheckBodyAuthority` 现在混入
`BorrowSummary` 与 `BodyLoanCheckResult` fingerprint、origin/dependency/loan/conflict count、
unknown/local-escape/diagnostic-emitted 等状态位，CLI incremental-cache subject collection 和 IDE snapshot query
collection 共用 `CheckedModule` 中的 borrow facts，不让 tooling/LSP 重新跑 borrow sema。IDE semantic facts 新增
`borrow_summary` 与 `body_loan_check`，fact detail 暴露 dependency count、unknown/local_escape、loan/conflict
count、diagnostic mode、first conflict reason 和 stable fingerprint；函数 hover 会展示
`borrow_summary=deps/unknown/local_escape`。enforced borrow diagnostics 现在包含 primary conflict、loan creation
note、invalidating action note，并在 CFG/liveness 能定位时补充 later carrier use note；按冲突点/range 的 cascade
suppression 保持。`dump_checked_module` 现在输出 `body_loan_checks` summary 与语义稳定 fingerprint；query/cache
fingerprint 不混入绝对 source range，诊断 range 仍保留在 checked facts 和 dump 中。
`BorrowEscapeAnalyzer` 继续保留，只有在新 checker parity 覆盖当前 borrowed-view escape matrix 后才降级或移除。
M7a 仍不包含完整 Rust-style lifetime surface、full Polonius Datalog engine、raw pointer alias safe proof、
用户级析构器语法、partial move / replace / take / swap 完整 place-level resource semantics、`dyn Trait`、
async drop 或 generator borrow。M7d-C 后续已补上窄 `impl Drop` / `deinit` semantic surface；M7d-D 后续已补上
静态 custom destructor call lowering。

2026-06-02：W7a release 性能与内存收口完成。普通 `--check` 路径只保留 body loan / borrow summary 等稳定
checked facts，不再长期保留 full `BodyFlowGraph`；checked/typed 输出和 IDE/tooling 仍可保留 CFG facts。函数级
`BorrowSummary` 对非借用返回类型走 fast path，只生成稳定空 return-dependency summary，不扫描完整函数体；
direct/trait call binding 查找改由 `CheckedModule` 维护 expr-id index，避免 summary 构建时反复全局线性扫描。
Release+LTO `perf-release-threshold` 已通过：5000 mixed generic、2M mixed AST、5000 mixed diagnostic 和 query
graph fuzz 均在阈值内，其中 2M AST lane 的 `sema.analyze` 已从 W7a 收口前的 200s 级风险回到秒级。

2026-06-02：M7-WP4 与 M7-WP5 已完成。`CheckedModule` 新增 `function_calls` 和
`borrow_summaries`，`src/sema/internal/sema_borrow_summary.cpp` / `.hpp` 在 return type inference、body-flow 和
local loan check 之后生成函数级 `FunctionBorrowSummary`，记录 parameter/local/temporary origins、return origin
dependency set、unknown-return 标志、local/temporary escape 标志和 stable fingerprint。普通函数、泛型函数、普通方法
和泛型方法调用会记录 direct call binding，call wrapper 可把 callee parameter dependency 映射到 caller 实参；
generic parameter 与 associated projection 按可能含借用处理，避免 `T = &U` 的泛型 wrapper 漏掉 parameter-origin
dependency。函数值调用、callee 缺 summary、raw/unchecked pointer path 和 callee local/temporary return 都走 conservative
unknown。`BodyFlowGraph` / `BodyLoanChecker` 同步补齐 `reinit`、`drop`、`cleanup_storage` action/conflict matrix：
整 local assignment 生成 `reinit`，field/index/deref assignment 保持 `write`，词法 block local cleanup 生成
`cleanup_storage` invalidation，并通过 carrier liveness 检查 cleanup 点是否仍有活跃 loan。`BorrowEscapeAnalyzer`
继续保留旧 borrowed-local escape 诊断，summary 同步记录 facts，等待后续 parity 后再降级/替换。

2026-06-02：M7-WP3 Phase 2/3 已完成 diagnostic-shadow + enforced local loan checker。`CheckedModule`
新增 `body_loan_checks`，按 `FunctionLookupKey` 保存本地 `Origin` / `Loan` / conflict facts、shadow/enforced
mode 和稳定 dump；`src/sema/internal/sema_body_loan_checker.cpp` / `.hpp` 在 Phase 1 `BodyFlowGraph` 上运行
deterministic worklist，使用 carrier-local liveness 支持直接本地 borrow 的 last-use 后写入，并用
projection-aware matrix 检查 active shared/mutable loan 与 write、owned-consume move、read、shared/mutable
borrow 的冲突。函数体分析已在 body-flow 收集后启用 enforced diagnostics，诊断包含 conflict primary 和 loan
creation note；`move_candidate` 只有在 M6 `OwnedUseMode::owned_consume` 时才作为 move 冲突。现有
`BorrowEscapeAnalyzer` 继续保留，M7-WP4 前不声称替代 borrowed-return contract 或跨函数 summary。

2026-06-02：M7 进入实现阶段，M7-WP2 Phase 1 已完成 collect-only BodyFlowGraph facts。`CheckedModule`
新增 `body_flow_graphs`，按 `FunctionLookupKey` 保存函数体的 point、edge、place 和 action facts；
`src/sema/internal/sema_body_flow_graph.cpp` / `.hpp` 使用迭代式 task stack 收集 statement/expression
entry-exit、顺序点、branch、return、call、defer cleanup、read/write/move-candidate 以及 shared/mutable
borrow action，并提供稳定 dump。函数体分析在现有 borrow escape 与 body move analysis 之后写入这些 facts，
当前不新增 diagnostics、不替换 `BorrowEscapeAnalyzer`、不改变 M6 move/resource/cleanup 行为。新增白盒测试覆盖
`&mut source.field` 的 projection place、call/return/cleanup facts、stable dump，以及完整 `analyze()` 路径写入
`CheckedModule`。

2026-06-02：M7 设计研究基线已完成，记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)。
执行路线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图](m7-roadmap.md)。该基线选择
Place/Origin/Loan/Point/BorrowAction/BorrowSummary 作为 M7 内部事实模型，明确 M7a 先做
CFG-sensitive loan liveness、projection-aware access conflict、borrowed-return contract 和函数摘要，不在第一步暴露完整
Rust-style lifetime surface，也不把 partial move、用户级析构器语法、unsafe/raw alias model 或 full Polonius engine
塞进 M7a。

2026-06-01：M6-WP2 到 M6-WP7 已完成 M6 实现基线。WP2 增加 compiler-owned `Copy`、
内部 `Discard` / `NeedsDrop` / ownership resource summary、结构化类型分类、stable resource fingerprint
和 checked dump 中的 deterministic resource summaries；`Drop` 仍然不是用户可写 bound。WP3 增加
expression owned-use side table、独立 whole-local move analysis 模块、迭代式 CFG/worklist
initialized / moved / maybe-moved dataflow、move 后重新初始化和 consume-origin diagnostics。WP4 增加统一
lexical cleanup-action stack，将 compiler cleanup 与 `defer` 交错，降低 drop flag，覆盖 normal
exit、overwrite、`return`、`break`、`continue` 和 `?` early return，并新增正式 IR `drop` / `drop_if` cleanup
节点、verifier 和 backend scaffold。WP5 和 WP6 增加 destructor body identity、stable drop-glue key、
target-independent drop-glue planner、IDE resource hover projection、generic parameter hover fallback 和
`aurex-lsp` stdio 入口。WP7 收口实现文档，并记录 M7 入口为 CFG-sensitive origin / loan / lifetime checker。
当前边界仍然
明确：当时 partial field move、indexed move-out、consuming pattern payload 和 non-`Copy` `?` payload transfer
都在正式负样例中拒绝；M7d-B 后续已补上本地 owned struct field partial move/reinit/drop flag 子集，M7d-C
后续已补上窄 `impl Drop` / `deinit` semantic surface，M7d-D 后续已补上静态 custom destructor call lowering。
aggregate rollback codegen 和完整 borrow checker 在该历史节点尚未实现。

2026-05-31：M6-WP1 已完成资源、值生命周期与访问语义的三轮设计审视。完整基线记录在
[Aurex M6 资源、值生命周期与访问语义调研和三轮设计审视基线](m6-resource-access-semantics-design.md)，
阶段路线记录在 [M6 资源、值生命周期与访问语义路线图](m6-roadmap.md)。第一轮比较 C++、Rust、Swift、
Mojo、Move、Zig、Go、Hylo、Pony、Verona、Cyclone、Lean、Koka、Roc、Linear Haskell、Idris 2、
Austral、Carbon、Clang 和相关研究；第二轮固定 Aurex 四维资源模型：`Copy`、`Discard`、`NeedsDrop`
和 future `MustConsume`，并选择 whole-local move、CFG-sensitive initialized state、词法 cleanup action
stack、`defer` 交错和 generic drop glue；第三轮通过 regex、owned container、文件、锁、FFI、覆盖赋值、
分支、循环、`?`、pattern、partial initialization、自引用、shared ownership cycle 和 future `dyn Trait`
案例压力测试边界。完整 borrow checker 后移到 M7。

2026-05-31：M5 已作为 default trait methods release baseline 收口，建立在已经收口的 M4 trait/protocol baseline
之上。
M5-WP1 输出是
[Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)
和 [M5 Default Trait Methods 路线图](m5-roadmap.md)。完整发布契约记录在
[Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md)。M5-WP2 已落地 syntax / AST / body-identity
基线：parser 接受 trait 内 default method body，prototype requirement 仍然显式保留，AST compact storage
和 AST dump 区分 `prototype` 与 `trait_default`，query identity 覆盖
`BodySlotKind::trait_default_method`。M5-WP3 和 M5-WP4 已经在 trait context 中只 type-check
trait-owned default body 一次，允许 impl 省略 defaulted requirement，保持 missing non-default requirement 报错，
并在 checked facts 中把 selected method origin 记录为 `impl_override`、`trait_default` 或 `param_env`。
M5-WP5 和 M5-WP6 现在已经把 selected inherited default 实例化为 concrete trait-owned method instance，
降低为 direct IR/LLVM call，通过 instance side tables 保持 associated-type substitution，IDE tooling 能区分
inherited default call 和 explicit override call 的 hover/definition，并通过 `BodySlotKind::trait_default_method`
给 synthetic default instance 提供稳定 incremental-cache/body-query category。M5-WP7 已收口 release baseline
docs、usage notes、version notes、unsupported matrix、validation matrix、常规仓库测试、coverage、query/cache gates 和
stress gates。
M5 的范围是 nominal static trait 上的 trait
method body、显式 method-origin facts、通过 `BodySlotKind::trait_default_method` 固定 trait-owned default
body identity、区分 explicit override 和 inherited default 的 impl completeness 规则、单态化后的 static
direct-call lowering，以及 selected origin 的 tooling / diagnostics / query projection。M5 明确不做
`dyn Trait`、vtable ABI、object safety、specialization、associated constants、default associated types、GAT、
blanket impl、RAII/resource semantics、Swift-style protocol extensions、Scala/Kotlin mixins 或 runtime
interface dispatch。设计调研已覆盖 Rust default trait methods、Swift protocol-extension dispatch 陷阱、Kotlin /
Java / C# default-interface 冲突规则、Haskell/GHC minimal-method 经验、Scala trait linearization 风险、Go
structural interface 反例、C++ inheritance/concepts 取舍和 MLIR interface-model 架构。

2026-05-31：M4 trait/protocol 系统已完成 WP1、WP2、WP3、WP4、WP5、WP6、WP7 和 WP8。M4-WP1 完成调研与设计基线，正式选择
nominal static trait：语言关键字为 `trait`，`protocol` 只作为行为契约的设计术语；conformance 由显式
`impl Trait for Type` 给出；泛型约束进入 canonical trait predicate / `ParamEnv`；调用默认静态分派，单态化后降低为具体
impl method direct call。M4-WP2 已把 `trait` / `impl Trait for Type` 的 token、parser、AST payload、AST
dump、lossless syntax 和 query identity scaffold 落到可回归基线。M4-WP3 已把 trait declaration 和 impl registry
接入 query-backed sema aggregate：`CheckedModule::traits` 记录 `TraitSignature`、generic params、visibility
和结构化 requirement；trait requirement prototype 不再作为普通 top-level function/prototype 校验；`CheckedModule::trait_impls`
记录 `impl Trait for Type` fact；sema 已校验缺方法、重复 impl method、未知 impl method、签名不匹配、trait
不可见、trait target 非命名 trait、self target 非命名类型、trait generic arity 和重复 exact impl key。M4-WP4 已在
`CheckedModule` 中新增 `TraitPredicate`、`TraitObligation`、`TraitEvidence` 和 `ParamEnvInfo`，把
`where T: Trait` 降低为正式 predicate；`Sized`、`Eq`、`Ord`、`Hash` 保持旧 capability 检查并同步记录
compiler-owned builtin trait predicate；generic instantiation 已按 ParamEnv 做 user trait candidate rejection；
trait impl registry 已增加 canonical coherence fingerprint、orphan rule 和 first-pass overlap check。M4-WP5 已完成
static trait method resolution and lowering：trait impl method 不进入普通 inherent method lookup；inherent method
继续优先；generic body trait call 通过当前 `ParamEnv` 绑定为 `param_env` `TraitMethodCallBinding`；concrete
receiver 通过 visible trait + impl registry 绑定为唯一 `impl` direct call；单态化后 LLVM IR 直接调用具体 impl method。
M4-WP6 已完成第一版 associated type model：trait body 支持 `type Item;`，trait impl body 支持
`type Item = Type;`，`Self.Item` 和 generic projection 降低为 canonical associated-projection type，
`Trait[Item = Type]` where predicate 降低为 trait predicate + associated type equality fact，impl method
requirement matching 会用 impl 给出的 associated type output 做替换，generic method call 可以通过 equality
predicate 归一化，并且 sema 已覆盖重复/缺失/未知 associated type、builtin equality constraint、缺 bound、shorthand
projection 歧义、projection cycle 和 equality unsatisfied 诊断。
M4-WP7 已完成 trait fact 的第一层 tooling / diagnostics 投影：IDE snapshot、ToolingSession 和 LSP
adapter 都能暴露 `where T:` 后的 trait completion、trait / trait method / impl method / associated type
的 hover/definition、trait 与 associated type 的 semantic-token 分类，以及基于 `DefKey` / `MemberKey`
的 rename identity。trait method 与 associated type member fact 已进入 workspace semantic index，diagnostics
现在会给 candidate impl、rejected candidate、associated-type equality mismatch、orphan check 和 overlap
位置补充 notes。

M5-WP1 到 M5-WP7 的测试和文档检查都已经落到常规仓库测试：`tests/gtest/sema/trait_tests.cpp` 覆盖
trait-owned default body checking、inherited default method facts、explicit override precedence、concrete
default method instance records、direct IR/LLVM lowering、generic reselection、associated-type normalization、
inherent-first priority 和 checked dump origin 字符串；`tests/gtest/tooling/ide_tooling_tests.cpp` 覆盖
inherited default 与 explicit override 的 IDE hover/definition；`tests/gtest/driver/cli_driver_tests.cpp`
覆盖 trait default method instance 的 incremental-cache rows；`tests/gtest/integration/documentation_tests.cpp`
检查 M5 release documentation contract；`tests/samples/checked/traits/trait_default_method_inherited.ax`
与 `tests/samples/checked/traits/trait_default_method_override.ax` 继续作为 checked-only origin dump fixture；
常规 positive samples `tests/samples/positive/traits/trait_default_method_inherited_dispatch.ax`、
`trait_default_method_override_dispatch.ax`、`trait_default_method_generic_dispatch.ax`、
`trait_default_method_associated_type_dispatch.ax` 和 `trait_default_method_inherent_precedence.ax` 覆盖 lowering；
`tests/samples/negative/traits/trait_default_method_return_mismatch.ax`、
`tests/samples/negative/traits/trait_default_method_self_field.ax` 和
`tests/samples/negative/traits/trait_default_method_missing_required.ax` 覆盖 default-body 诊断和 completeness 规则。

M4-WP3/WP4/WP5/WP6/WP7 的测试已落到常规仓库测试，而不是临时目录：`tests/gtest/sema/trait_tests.cpp` 覆盖白盒 checked facts、dump
和正负样例，`tests/samples/positive/traits/trait_impl_registry.ax` 覆盖正样例，`tests/samples/negative/traits/*.ax`
覆盖诊断路径，`tests/samples/positive/traits/trait_predicate_where_generic.ax`、
`tests/samples/negative/traits/trait_predicate_unsatisfied_generic_arg.ax` 和
`tests/samples/negative/traits/trait_impl_orphan_external.ax` 覆盖 WP4 predicate/candidate/orphan 路径，
`tests/samples/positive/traits/trait_method_static_dispatch.ax`、
`tests/samples/positive/traits/trait_method_associated_static_dispatch.ax` 和
`tests/samples/positive/traits/trait_method_inherent_precedence.ax` 覆盖 WP5 正向 receiver direct-call、associated/static
direct-call 和 inherent-first 路径，
`tests/samples/positive/traits/trait_method_function_field_precedence.ax` 覆盖 function-valued field call 回退不被
trait missing diagnostic 抢占，
`tests/samples/negative/traits/trait_method_ambiguous_bound.ax`、
`tests/samples/negative/traits/trait_method_ambiguous_impl.ax`、
`tests/samples/negative/traits/trait_method_associated_missing_impl.ax`、
`tests/samples/negative/traits/trait_method_missing_bound.ax` 和
`tests/samples/negative/traits/trait_method_missing_impl.ax` 覆盖 WP5 负向诊断，`tests/samples/imports/samplelib/traits.ax`
覆盖跨模块可见性；`tests/samples/positive/traits/trait_associated_type_basic.ax` 和
`tests/samples/positive/traits/trait_associated_type_where_equality.ax` 覆盖 WP6 projection、impl output
substitution、equality predicate 和 runtime lowering；`trait_associated_type_*.ax` 负样例覆盖 ambiguity、duplicate
equality、builtin equality、trait/impl associated item 重复、impl output 缺失、未知 impl/equality 名称、GAT
拒绝、缺 bound、projection cycle、签名不匹配和 equality unsatisfied。
`tests/gtest/tooling/ide_tooling_tests.cpp` 和
`tests/gtest/tooling/session_lsp_tooling_tests.cpp` 覆盖 WP7 trait completion、hover/definition、semantic
token、rename/member identity、workspace member indexing、LSP projection 和 diagnostic notes。完整设计见
[Aurex M4-WP1 Trait / Protocol 系统调研与设计基线](m4-trait-protocol-system-design.md)，阶段路线见
[M4 Trait / Protocol 系统路线图](m4-roadmap.md)。已完成的 M4 release contract 记录在
[Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md)。

M4-WP8 已完成 release baseline 收口：roadmap/progress/next-steps、release baseline 文档、语言表面说明、
unsupported matrix、常规仓库测试、coverage、query/cache/profile stress gates 和后续入口现在一致确认 M4
完成面是 nominal、explicit、static-dispatch-first。M4 后续工作应作为独立设计流启动。

当前仍未把 M4 误扩成完整 dynamic trait 系统：dynamic trait object、vtable ABI/object safety、associated
constant、specialization、generic associated type 和 RAII/resource semantics 仍由后续独立设计承接。WP6 的
`where` grammar 已支持 identifier trait predicate 上的 associated-type equality，但 qualified where predicate
和 generic trait predicate arguments 仍留给后续 solver 阶段。

当前仓库已经从 M2 language-core-no-std 基线进入 M2.5 frontend-foundation 阶段。M2 的目标不是继续修补 M1，而是重新收口语言核心：冻结并删除标准库和 M1 系统样例，把注意力放回基础语法、类型系统、模式匹配、`unsafe` 边界、IR 和 LLVM 后端。M2.5 建立在这条已收口主线之上，开始处理 query 化、lossless syntax 和 IDE-native 前端所需的结构化地基。

2026-05-25 R5 Compilation Pipeline / Driver Action core 已完成。M2.5 前端地基和收尾拆分完成后，重构主线已完成现代编译器 driver/session/pipeline 边界：`CompilerInvocation` 保持纯配置，`Compiler` 退回 public facade，一次编译的 source、diagnostics、profile、cache policy 和 backend emitter 由内部 `CompilationSession` 持有，source/frontend/sema/cache/lowering/backend 阶段由 `CompilationPipeline` 显式编排。R5.2 已把 source/token/lossless/module graph/AST dump/sema/cache write 收口到内部 `FrontendPipeline`；R5.3 继续把 checked dump、IR lowering、IR pass pipeline 和 IR dump 收口到 `LoweringPipeline`，把 LLVM IR emission、LLVM IR dump、temporary LLVM file、clang native invocation 和 native 输出路径校验收口到 `BackendPipeline`，并用 `PipelineStage` 固定 driver 主阶段的 profile/input/output/diagnostic/cache-query 契约；R5.4 已建立轻量 IR pass manager、`PassResult`、`PreservedAnalyses` 和 verifier gate；R5.5 已建立 `ModuleAnalysisManager`，惰性缓存 CFG、dominance 和 value-use analysis，并按 `PreservedAnalyses` 自动失效；R5.6 已给 IR verifier gate failure 接入稳定 `stage/profile/verifier/pass` 上下文，同时保持原始 verifier body 和 `ErrorCode` 不变，并让 `LoweringPipeline` 从 `PipelineStage` record 传递 IR pass pipeline 阶段名；R5.7 已让 profile JSON 为 driver 主阶段 phase 输出来自 `PipelineStageRecord` 的 `stage` 元数据对象；R5.8 已让 incremental-cache profile 子事件通过 `parent_stage` 挂回 `incremental_cache.lookup` 或 `incremental_cache.write`；R5.9 已让 `DiagnosticCategory` 通过 `PipelineStage` 反查候选 owner stage，lexer 诊断保留 `tokens.lex` / `module.lex` 双归属，sema 类诊断归属 `sema.analyze`，且 diagnostics text/JSON 协议保持不变；R5.10 已让 `aurex_tooling` 的 `IdeDiagnostic.owner_stages` 消费同一份 `PipelineStageRecord`，供后续 LSP/IDE 阶段视图直接复用；R5.11 已把阶段目录头文件提升为公开只读 API，并用 `PipelineStageMetadata` 统一 profile writer 和 tooling diagnostics 的 metadata 形状；R5.12 已让 profile 记录入口直接接收 `PipelineStageId` / `PipelineProfileSubeventId`，调用点不再散落阶段 profile name 字符串；R5.13 已增加 `pipeline_profile_phase_classification(...)`，profile JSON writer 和后续 viewer/LSP adapter 可以通过同一入口区分 driver 主阶段、profile 子事件和 unknown。M3 后续实现继续复用 R5 稳定下来的 driver/session/query/diagnostics/pipeline 主路径。

2026-05-28 M3.0 Phase 9A-D 已完成模块系统收口：文档已把 M3.0 contract matrix 固定为
ModuleKey/ModulePartKey、part root、package visibility、source-root topology、IDE source-part context、
selective re-export 和 query/cache 边界；IDE/tooling 对真实 `.parts/<name>.ax` buffer 可以在 owning primary
存在且显式列出该 part 时恢复 resolved `ModulePartKey`，无法证明 ownership 时继续保持 unresolved；primary-level
`pub use module.Item [as Alias]` / `pub(package) use ...` 已作为 selective item re-export 进入 parser、AST、
loader、sema、ModuleGraph、ModuleExports 和 ModulePackageExports。M3.0 仍明确拒绝 glob import/use、
part-local `pub use`、bare/private use、nested module tree、`pub(in path)`、file-private、workspace/dependency
resolver、lockfile、version solving 和 package manager。

2026-05-28 M3.1 Generics Completion 已完成 release baseline：`GenericTemplateSignature`、
`GenericInstanceSignature` 和 `GenericInstanceBody` 成为泛型权威边界；generic ABI suffix、stable id 和
incremental key 来自 `GenericInstanceKey` / canonical type identity；generic body、IR lowering、
LLVM lowering 和 native execution 消费同一份实例身份与 side table 视图；generic builtin type operand
和 value-only builtin 在 generic function 中完成 sema/IR/LLVM 闭环；method-local generics 已进入
ABI/query/diagnostics/lowering/native 路径。2026-05-29 `m3.1` 已 fast-forward 合并回 `m3`，随后
`m3.2` 完成 Query-backed Sema 设计和实现。M3.2 已把 sema 从“单次 eager analyzer 产生 checked module”
推进为 query-backed semantic authority：`ItemSignature`、`BodySyntax`、
`TypeCheckBody`、`GenericTemplateSignature`、`GenericInstanceSignature` 和 `GenericInstanceBody`
形成统一 authority 边界，`CheckedModule` 分清 durable facts、session-local caches 和 lowering-only side
tables，incremental cache / query pruning / provider-skip replay 能解释 sema 级结果复用。M3.2 执行记录已收束为
[Aurex M3.2 Query-backed Sema 设计与执行计划](m3.2-query-backed-sema-plan.md)，后续新专题应进入新的 M3.3 /
LSP adapter / 更细粒度 incremental sema 计划，不再向 M3.2 追加新范围。

2026-05-29 M3.2 WP-1/2/3 Query-backed Sema authority batch 已完成：非泛型
`ItemSignature`、`FunctionBodySyntax` 和 `TypeCheckBody` 已补齐到 M3.1 泛型 authority 的同级边界。
`ItemSignatureAuthority` 显式记录 signature incremental key、`ModulePartKey`、namespace、`DefKind`、
visibility rank、value/generic 参数数量、return/receiver/unsafe/variadic/definition flags；
`FunctionBodySyntaxAuthority` 记录 body syntax fingerprint、owner `DefKey`、`ModulePartKey`、body source
range、body slot 和 ordinal；`TypeCheckBodyAuthority` 记录 checked body fingerprint、body syntax result、
item signature result、side-table summary、coercion count、retained-side-table flag 和 diagnostics flag。
provider input 不再接收非泛型 item/body 的裸 `IncrementalKey` 或裸 body fingerprint，provider 默认实现、
provider-skip replay、incremental-cache subject ordering 和 `query_record_for_subject` 共用同一套 authority
result helper。`CheckedModule` 当前仍作为 eager sema 聚合结果，但 durable sema facts 的 materialization 输入
来自 stable id、incremental key、module id、part index、body range 和 side-table summaries；跨 session 的事实
由 query record/cache 保存，lowering-only side table 仍留在 checked aggregate 内。新增/更新 query、robustness
和 driver cache 覆盖 authority valid/invalid、语义敏感 fingerprint、依赖边、split logical module package rows
和手工 query record fixture。

2026-05-29 M3.2 WP-4/5/6 已完成并收口 Query-backed Sema 当前批次：新增
`SemanticLookupService`、`SemanticTypeService`、`SemanticGenericService` 和 `SemanticBodyCheckService`
作为 `SemanticAnalyzerCore` 的内部 service boundary；pipeline 的 generic definition、ordinary function body
和 type layout validation 已通过这些 service 进入现有 analyzer/resolver，不复制 analyzer state、不重写语义算法。
`aurex_tooling::IdeSnapshot` 现在在同一个 `query` snapshot 中暴露 module/item/signature/body/type-check
query records、dependency edges 和 `semantic_facts`；semantic facts 携带 stable `DefKey` / `MemberKey` /
`BodyKey` / `GenericInstanceKey`、source range、part index 和 checked 标记。IDE semantic module identity
已对齐 sema stable module identity，避免 tooling `ModuleKey` 与 checked stable def key 分叉。M3.2 当前
work package 已全部完成，后续若继续推进，应以新的 M3.3 / LSP adapter / 更细粒度 incremental sema 计划为入口。

2026-05-29：M3.2 已 fast-forward 合并回 `m3`，M3.3 Tooling Session And Incremental Sema 也已完成并合并回
`m3`。M3.3 收口基线包括协议无关 `ToolingSession`、versioned open-document state、`IdeSnapshot`
snapshot cache、in-place snapshot 构建入口、session-level diagnostics/hover/definition/reference wrappers、
最小 `LspServer` JSON-RPC adapter、document symbols、incremental reuse planning、workspace semantic indexing
和 quality gates。LSP 层只消费 tooling value types，不读取 parser/sema/query/driver internals。

2026-05-29：`m3.4` 已从 M3.3 收口基线切出。新的执行入口是
[Aurex M3.4 Real Incremental Sema Execution 计划](m3.4-real-incremental-sema-plan.md)。M3.4 的优先级是把
M3.3 reuse explanation 变成 executable semantic fact reuse：snapshot construction 必须接收 previous
snapshot/query context，query reuse decision 必须驱动局部重算，semantic facts 必须在 body-local /
signature / module edits 后保持稳定，workspace index 尽量按 affected fact identity 更新。完整 M3.4-M3.9
路线已写入 [M3 路线图](m3-roadmap.md)。

2026-05-29：M3.4 Real Incremental Sema Execution 已完成当前 deterministic tooling/query 边界。
`IdeIncrementalSnapshotInput` 会把 previous query snapshot 传入 `build_ide_snapshot_into(...)`；可证明
unchanged 的 file/lex/parse/diagnostics、module-surface、item-signature、generic-template-signature、
function-body-syntax 和 type-check-body records 会在 provider evaluation 前 seed 到 `QueryContext`。
`ToolingIncrementalSnapshotResult` 暴露已执行的 reuse plan、reuse-execution counters 和 workspace-index
update stats；`ToolingWorkspaceSemanticIndex` 报告 retained、replaced、removed、inserted facts，并避免对外返回
旧 document version 的 stale entries。聚焦测试覆盖 accepted/rejected previous context、重复 body-local edit
稳定性、removed-definition invalidation、generic body-edit reuse、malformed reuse 和无旧版本泄漏的 workspace
facts。该阶段已收口，后续目标已经推进到 M3.5/M3.6。

2026-05-29：M3.5 Incremental Syntax And Stable AST Identity 已完成当前 deterministic tooling/syntax 边界。
`ToolingSession` 新增 range-based edit 入口，`ToolingDocumentTextEdit` 能描述 begin / removed length / inserted
text，并在 `change_document_range_with_reuse_plan(...)` 中返回 applied edit、精确 edit impact、reuse plan 和
incremental snapshot result。`LosslessNodeStableKey` 已作为位置无关 syntax identity 落地，不使用绝对 source
range 或 token index；`compare_lossless_stable_nodes(...)` 通过 stable-key multiset 报告 reused、recomputed、
invalidated 和 collision counters；这些结果进入 `ToolingIncrementalSnapshotResult::syntax_reuse`。新增
`IdeAstNodeInfo` / `ToolingAstNode` 将 offset 投影到 AST item 或 function body，并输出稳定 `DefKey` /
`BodyKey`，使 offset-to-token、syntax-node、AST-node 和 semantic-fact projection 能在同一个 snapshot 中对齐。
M3.6 Project Graph And Persistent Query DB 已完成，下一目标是 M3.7 IDE Semantic Features。

2026-05-30：M3.6 Project Graph And Persistent Query DB 已完成当前工程级 query/cache 边界。
`ProjectModel` / `WorkspaceModel` 已作为 `aurex_project` 公共目标落地，driver invocation 和
`ToolingSession` 都消费同一套 package root、source root、import roots、target config、command options
和 open buffers 输入。query 层新增 `ProjectKey`、`QueryKind::project_graph` 和 project graph provider；
`module_graph` 现在显式依赖 project graph 和 module part queries。incremental cache schema 升到 2，
header 写入 project identity、package/source root、target config、command options 和 open buffer count；
`incremental_cache.project_inputs` profile 子事件能说明 project input 是 reuse 还是 reject，以及具体 changed
inputs。测试覆盖 project graph key/layout/provider、edge verifier、driver cache row/edge/profile 和 tooling
workspace model open/change/close 行为。详细计划见
[Aurex M3.6 Project Graph And Persistent Query DB 计划](m3.6-project-graph-persistent-query-db-plan.md)。

2026-05-30：M3.7 IDE Semantic Features 已完成当前协议无关 IDE 能力第一层。
`IdeSnapshot` / `ToolingSession` 新增 completion、rename、semantic tokens、inlay hints、code actions 和
workspace symbols value types；completion 合并 syntax context、sema scope、keyword 和 open workspace
semantic facts；rename 基于 symbol identity 生成 workspace edit plan，并检查 identifier、reserved keyword
和可见符号冲突；semantic tokens 由 syntax token kind 加 checked symbol facts 合成；inlay hints 当前覆盖缺少显式
类型标注的 checked locals；code actions 从结构化 help diagnostics 生成 lookup suggestion quick fix。
LSP adapter 只做投影，新增 `textDocument/completion`、`textDocument/rename`、
`textDocument/semanticTokens/full`、`textDocument/codeAction`、`workspace/symbol` 和
`textDocument/inlayHint`，并在文档请求边界使用 generation guard 防止 stale result 发布。详细计划见
[Aurex M3.7 IDE Semantic Features 计划与收口记录](m3.7-ide-semantic-features-plan.md)。

2026-05-30：M3.8 Query-backed Lowering / Backend Reuse 已完成当前 lowering/IR/backend reuse 边界。
`lower_function_ir` query rows 现在只在真实 lowering 和 IR pass pipeline 完成后写入 incremental cache；
普通函数体通过 `BodyKey`，generic function instance 通过 `GenericInstanceKey` 映射到 lowered IR function
symbol，并用真实 target-independent IR unit fingerprint 生成 result。新增 `ir::layout_abi_fingerprint(...)`、
`ir::function_ir_unit_fingerprints(...)` 和 `ir::llvm_emission_unit_fingerprint(...)`，把 type layout、
payload enum layout、ABI symbol、target-independent IR unit 和 LLVM emission unit 分成可观测的稳定事实边界。
`PassPipelineRunSummary` 现在记录 invalidated analyses，`ir.pass_pipeline` profile detail 输出
scheduled/executed/changed/preserved/invalidated 摘要；`llvm.emit_ir` profile detail 输出 function unit 数量和
layout/ABI fingerprint global id。`check` / `typed` / `checked` 仍不写 lower IR rows，`ir` / `llvm-ir` /
native emit 在 lowering/pass pipeline 后写真实 lower IR rows。详细收口记录见
[Aurex M3.8 Query-backed Lowering / Backend Reuse 计划与收口记录](m3.8-query-backed-lowering-backend-reuse-plan.md)。

2026-05-30：M3.9 已完成完整 M3 release baseline 收口。`m3` 分支现在包含 M3.0 到 M3.8 的全部阶段成果，
并新增最终 authority-boundary audit 和质量门基线。固定的公开边界是：source/lex 产出 source facts；
parse/syntax 持有 AST 和 stable syntax identity；module/project 持有 `ModuleRecord`、`ModulePartKey`
和 project graph facts；sema 持有 query-backed durable checked facts；tooling 通过 `IdeSnapshot` 和
`ToolingSession` 消费事实；LSP 只作为 adapter；lowering 持有 verified Aurex IR 和 IR unit fingerprints；
backend 消费 optimized Aurex IR，不回读 AST 或 sema 私有状态。详细收口记录见
[Aurex M3.9 M3 Release Baseline 与 Authority Audit](m3.9-m3-release-baseline.md)。

2026-05-28 WP-1B Generic Instance Identity Propagation 已完成：`FunctionSignature`、`EnumCaseInfo`、
`GenericEnumInstanceInfo` 和 `GenericTypeAliasInstanceInfo` 都携带结构化 `GenericInstanceKey`；
generic function / owner-generic method 的 retained 与 non-retained 路径都会把 identity 写入 checked
signature；generic type alias 实例保存 resolved target type 和对 target type 敏感的 instance signature
incremental key，但仍保持透明别名语义；incremental cache 的 generic instance signature subject 从 checked
metadata 收集并按 key 去重，invalid key 不再进入 query subject。新增白盒覆盖 function signature identity、
generic enum case identity、generic type alias instance identity、checked module copy/move 保真和 driver cache
rows。

2026-05-28 WP-2 Generic Query Authority 已完成：query provider input 现在携带
`GenericTemplateSignatureAuthority`、`GenericInstanceSignatureAuthority` 和 `GenericInstanceBodyAuthority`，
不再只把泛型 query result 绑定到裸 `IncrementalKey` 或 body fingerprint。template authority 记录 signature、
`ModulePartKey`、namespace、visibility rank、param count 和 constraint count；instance signature authority
记录 instance kind、type/const args、param-env predicate count、value/generic param counts、return/receiver、
unsafe/variadic/definition flags；body authority 记录 checked body、signature result、generic side-table layout
count、sparse fallback count 和 retained/local-dense/sparse flags。incremental cache subject 从 checked metadata
和 module records 构造这些 authority，provider、provider-skip replay、query pruning 和 `query_record_for_subject`
共用同一套 result fingerprint helper。generic struct / enum 实例的 upstream signature fingerprint 同步补强为
解析后形状敏感，struct 字段和 enum payload 在同一个 `GenericInstanceKey` 下变化时也会改变 signature result。
新增 query/sema/driver 覆盖 authority valid/invalid、fingerprint 语义敏感性、generic signature/body dependency、
fallback/cycle、generic aggregate shape、generic cache rows、query pruning reuse 和 malformed graph / identity
repair。

2026-05-28 WP-3 Generic Body And Lowering Closure 已完成：retained generic function instance 现在在
`GenericFunctionInstanceInfo` 中显式保存 `body`，`CheckedModule::generic_function_instance_body_view(...)`
把 instance、signature、side table、AST item 和 body 组成唯一的 sema-authority 视图。IR lowerer 的 generic
declaration/body lowering 改为消费该 view，`lower_ast` 在缺失 retained body view 时直接返回 internal error，
不再静默跳过或在 lowerer 侧重新解释 generic identity。incremental cache 的 generic body checked-result
fingerprint 改为读取 retained body 对应 AST block range，`--emit=typed` 只写 generic body query rows，不再写
lower generic IR rows，IR/LLVM/native emit mode 才收集 lower generic instance IR subject。新增/更新 sema、IR
whitebox、driver cache 和 sample runtime 覆盖 retained/discarded emit-mode 边界、generic body view、缺失 view
拒绝、checked module copy/move body 保真和 `generics/basic_m2.ax` native execution。

2026-05-28 WP-4 Generic Builtin Operand Closure 已完成：generic function / method instance body 的
`GenericAnalysisScope` 在 retained 与 non-retained side-table 路径都缓存 syntax type handle，`sizeof[T]` /
`alignof[T]`、`ptrat[*const T]`、`ptrcast[*const T]` 和 `bitcast[*const T]` 这类 builtin type operand
在实例化后由 sema 写入 concrete type，再由 IR lowering 消费同一份 retained side table。`ptraddr`、
`sliceptr`、`slicelen`、`strptr`、`strblen`、`strvalid`、`strfromutf8` 和 `strraw` 已通过泛型正样例覆盖
retained expression side table 路径。新增 `generics/builtins_m3_1.ax` 的 IR dump 和 native smoke 覆盖，
并新增缺 `Sized` 的 `sizeof[T]`、非法 `ptrat[T]` target 负样例。`cast[T](value)` 仍受现有 scalar-cast
规则限制，M3.1 不引入新的 `Scalar` / `Cast` capability。

2026-05-28 WP-5 Method-local Generics 已完成：`impl[T] Owner[T]` 的 owner 参数继续作为 method
`generic_params` 前缀，method-local 参数保持独立 `GenericParamIdentity`；sema 注册阶段不再拒绝
method-local generic，普通方法调用可从参数推断局部泛型，显式 `value.method[T](...)` / `Type.method[T](...)`
只绑定 method-local 参数。generic method instance 现在使用 `DefNamespace::member` 的 `GenericInstanceKey`
和生成 semantic key / ABI suffix，避免同一 owner 上不同 method-local 实参实例发生 lookup 或 C 符号碰撞；
实例不再写入普通 method-name lookup，调用解析始终回到 template bucket。IR lowering 已支持
`generic_apply(field(...))` 显式泛型方法 callee 并正确补 receiver 参数。新增
`generics/method_local_m3_1.ax` 正样例和 arity、无法推断、where 不满足、非泛型方法误传 type args 负样例；
原 “method-local generic unsupported” 回归测试已更新为新语义 checked dump 覆盖。

2026-05-28 WP-7 Generic Closure Audit And Release Baseline 已完成，M3.1 泛型闭环进入可验收基线：
审计确认 `generic_instance_abi_suffix` 只接收 `GenericInstanceKey`，generic struct / enum / type alias /
function / owner-generic method / method-local generic method 的 stable id、ABI suffix、incremental key 和
query subject 都从 `GenericInstanceIdentity` 或 checked metadata 中的结构化 `GenericInstanceKey` 派生。
`generic_instance_key_suffix` 仍可使用 session-local `TypeHandle.value`，但只作为本次编译的 lookup/cache fast
key；白盒测试已经验证它跨 session 可不同，而 stable instance key 和 ABI suffix 相同。checked dump、
diagnostics、IR dump 中的 display string / c_name 只作为展示输出，incremental cache generic
signature/body/lower-IR subjects 从 checked metadata 和 authority 结构收集并按 `GenericInstanceKey` 去重。
新增 `generics/method_local_identity_closure_m3_1.ax` 和 runtime smoke 覆盖同一 owner-generic 类型上的
owner-only method、method-local method，以及相同 method-local type args 跨不同 owner instance 的 native
行为。M3.1 当前 release baseline 明确不包含用户 trait、associated type、const generic、resource
capability、RAII、closure、async/generator/iterator 或标准库重建。

M1 阶段已经舍弃。主要原因不是单个功能失败，而是整体设计方向不稳：

- 标准库、host support、构建工具样例和语言核心同时扩张，导致测试结果很难判断是语言问题、库问题还是工具链问题。
- 部分能力仍缺少语言级规则，例如资源释放、容器 API 约束，以及未来标准库 `Result` / `Option` 定义和当前结构化 `?` shape 之间的正式绑定。
- 语法地基还没有冻结时就推进 selfhost/build-tool 路线，造成前端、库 API、资源模型互相牵制。
- M1 的很多样例验证的是“能跑通当前 demo”，不是验证语言核心是否稳定、可解释、可长期扩展。

因此 M2 只承认当前 C++ Stage0 编译器、Aurex IR、LLVM backend 和自包含语言样例作为有效基线。当前仓库没有 `std/` 目录，也没有 `selfhost/` 目录；相关旧路线只作为历史输入，不再代表当前进度。

## 已完成能力

当前生产编译器位于 `src/` 和 `include/`，使用 C++20 实现。

- CLI 支持 `--check`、`--dump-*`、`--emit=*`、`--opt-level`、`-I`、`-o`、`--clang` 和 `--clang-arg`。
- driver 能完成文件 IO、模块加载、编译流水线调度、IR lowering / pass pipeline、LLVM IR emission、
  LLVM IR 临时文件生成和 clang 调用；当前主路径已经拆成 `CompilationSession`、`CompilationPipeline`、
  `FrontendPipeline`、`LoweringPipeline`、`BackendPipeline` 和 `PipelineStage` 阶段记录。
- module loader 支持根模块和 import 模块合并，能检测模块名不匹配、缺失 import、循环 import、重复模块名和重复加载。
- lexer/parser 是手写实现，输出 ID-backed AST；token、AST、module、checked、IR、LLVM IR 都有 dump 路径。
- 语义分析已具备类型表、符号表、函数签名、ABI 名称、struct/enum 元数据、泛型实例化、表达式类型表和 source range 诊断；诊断分级支持 error/warning/note/help，lookup miss 已有 `did you mean` help，常见 type mismatch 输出 expected/actual note，duplicate 主路径输出 previous declaration note，parser 成对 delimiter 缺失输出 opening delimiter note。M2.5 的首项工作已完成：sema 诊断在创建时携带显式 kind/category/code，不再从 message 文本反推机器元数据。
- 当前语言样例覆盖函数、函数指针类型、`extern c`、`export c fn`、普通 `fn main`、import、可见性、const、type alias、struct、enum、opaque struct、泛型、method、指针、数组、slice、字段访问、索引、cast、内建 size/align、`if` 表达式、block 表达式、`match`、`while`、C-style `for`、基础 `for i in range(...)`、`defer` 和 `?`。
- 程序入口支持根模块普通 `fn main`，签名包括无参数或 `argc/argv`，返回 `i32` 或 `void`。
- Aurex IR 是 typed CFG/SSA-like 中间层，后端只消费 IR，不回读 AST。
- IR verifier 会检查函数、block、value、terminator、类型和引用一致性。
- pass pipeline 支持 `O0` 到 `O3` 选项；当前实际优化以保守的局部 mem2reg 和 CFG cleanup 为主。
  IR pass manager 已提供 `ModulePassManager`、`PassResult`、`PreservedAnalyses`、`VerifierGate` 和
  `run_pass_pipeline_with_summary`，旧 `run_pass_pipeline` 调用保持兼容；`ModuleAnalysisManager` 已提供 CFG、
  dominance 和 value-use 缓存与 pass 后失效；verifier gate failure 现在携带稳定 stage/profile/verifier/pass
  上下文，driver 从 `PipelineStageId::ir_pass_pipeline` record 传入阶段名，原始 verifier 错误 body 保持不变。
- `aurex-profile-v1` 的 driver 主阶段 phase 附带可选 `stage` 元数据对象，包含 stage id、input、output、
  diagnostic ownership 和 cache/query impact；原 phase name 和内部 incremental-cache query 子事件保持不变。
  incremental-cache 子事件附带可选 `parent_stage` 元数据，profile viewer 可以把 source-stage reuse
  归回 `incremental_cache.lookup`，把 query diff / plan / pruning / provider-eval 归回
  `incremental_cache.write`，但这些子事件仍不是 driver 主阶段。`PipelineStage` 同时提供
  `DiagnosticCategory` 到候选 owner stage 的反查；`IdeDiagnostic.owner_stages` 已通过公开
  `PipelineStageMetadata` 消费这些记录；profile 主阶段和 incremental-cache 子事件也通过
  `PipelineStageId` / `PipelineProfileSubeventId` 进入 profiler；profile phase 消费端通过
  `pipeline_profile_phase_classification(...)` 统一分类，避免后续 IDE/LSP 阶段视图重新维护 phase-name
  或诊断阶段表。
- LLVM backend 使用 LLVM C++ API 从 Aurex IR 生成 LLVM IR，并通过 clang 生成 asm、object 和 executable。

## 已删除能力

M2 明确删除并暂缓这些 M1 内容：

- `std/` 源树。
- host C support 源文件和自动链接。
- driver 的标准库查找、import path 自动注入和安装后 std 查找。
- CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 安装规则中的 `share/aurex/std`。
- std/M1/system/build-tool 样例。
- 依赖标准库名字的资源语义特判。
- selfhost / Stage1 / AIR snapshot 路线在当前仓库中的实现。

这些内容不是永远不能回来，而是必须等基础语法、module/package、`unsafe`、slice/string 和泛型约束稳定后再重新设计或重新评估；拥有型资源库还要等后续资源语义专题完成。

## 质量门

当前主要质量门：

```sh
tools/run_tests.sh
tools/bench.py
make perf
make perf-stress
make perf-stress-threshold
make perf-release-threshold
make perf-ast-stress
```

测试覆盖：

- lexer/parser/AST dump。
- driver/CLI/native toolchain。
- positive/negative `.ax` sample suite。
- 模块、可见性、泛型、函数、方法、pattern matching、error handling 和类型系统诊断。
- Aurex IR lowering、IR verifier、pass pipeline、LLVM backend。
- native execution 和安装后 compiler 执行。

`tools/bench.py` 使用 Release `build/perf` 构建目录，并用 Google Benchmark
测量 frontend 热路径。`make perf` 输出基于 JSON 的 Aurex frontend baseline，
覆盖 lexer、lookup-heavy sema、generic-instantiation-heavy sema 和 AST bulk sema 路径，并运行
Google Benchmark 的进程级现代前端对比通道，对可用的 `clang++`、`g++`、`rustc`
做 frontend/check 模式基线；`make perf-compare` 只运行跨前端对比通道。
`make perf-stress` 运行 `tools/generic_stress.py`、`tools/ast_stress.py` 和
`tools/diagnostic_stress.py`，生成 mixed-feature 泛型、AST bulk 和 diagnostic
源码，并记录 `aurexc --check` elapsed time + peak RSS baseline；`make perf-ast-stress`
只运行 AST bulk RSS/time lane。三条 stress lane 默认使用 `--shape=mixed`：generic lane 覆盖
generic struct/enum/type alias、where constraint、impl method、pointer alias、tuple/pair、slice
和 pattern matching；AST lane 覆盖 extern、type alias、struct/enum、impl/method、generic
constraint、tuple、array/slice、match/or-pattern、`let else`、`if is`、`try`、`defer`、
for/while/range、compound assignment、unsafe pointer/string builtin，并在 2M bulk 中抽样保留复杂
feature block；diagnostic lane 循环覆盖 unknown name、type mismatch、call arity/type、field/index、
struct literal、enum payload、builtin、generic apply、array/void、operator 和 match-arm 类型错误。
`tools/generic_stress.py` / `tools/ast_stress.py` / `tools/diagnostic_stress.py` 现在支持
`--max-elapsed-ms`、`--max-rss-mib`、`--threshold-profile` 和 `--threshold-scale`；三条 lane 共用
`tools/perf_thresholds.py`，把 raw thresholds、profile、scale、machine info、effective thresholds、
进程级 wall/user/sys/RSS/page fault 指标、以及 `aurexc --profile-output` 产出的
`aurex-profile-v1` 阶段 profile 写入 JSON。`make perf-stress-threshold` 默认跑 100/200 generic +
1000/5000 AST bulk + 100/500 errors 的轻量阈值门，GitHub Actions `stress-thresholds` job 固定
`AUREX_PERF_THRESHOLD_PROFILE=github-ubuntu-24.04-fedora`；`make perf-release-threshold` 现在默认用
独立 `build/perf-lto` + `AUREX_STRESS_ENABLE_LTO=ON` 跑 5000 generic、2M AST 和 5000 errors 的
Release+LTO 发布阈值门。`make perf-release-lto-threshold` 和 `make perf-release-all-threshold`
保留为同一发布阈值门的兼容别名，不再重复跑普通 Release 与 Release+LTO 两套逻辑；发布 AST RSS 阈值为
8192 MiB，用来保留高复杂 mixed 源码而不是降级成过渡态 toy case。M6-WP2/WP3 的 5000 mixed generic
Release+LTO 曲线实测为：1000/2000/3000/4000/5000 实例峰值 RSS 约
132.5/239.0/348.3/455.0/560.0 MiB，parse 后 RSS 约 77.5/135.0/192.1/250.0/307.0 MiB，
sema delta 约 53.2/103.9/156.1/204.9/252.9 MiB，端点斜率约 `+106.9 MiB / 1000`
实例，其中 parse 约 `+57.4 MiB / 1000`、sema 约 `+49.9 MiB / 1000`；因此发布级
generic RSS 阈值校准到 640 MiB，轻量 100/200 generic gate 仍保留 512 MiB。
generic function instance 签名、
generic struct/enum `TypeInfo` 和 checked enum case display 已经把内部 semantic key / TypeHandle
args 和展示名分离，`--check` 热路径不再为了 checked signature 或泛型类型实例生成
`id[i32]`、`Box[i32]`、`Maybe[i32]_some` 这类展示字符串，dump、IR lowering 和诊断需要时再延迟格式化。
`--check` 模式不再保留泛型实例 side table；`--emit=typed` 会保留 typed generic body 但不做 lowering，
用于把 retained-side-table 内存和 IR/codegen 成本分开压测。`tools/generic_stress.py --shape=templates` 覆盖 2000/5000+ 不同泛型模板场景；IR/native 输出模式继续保留 lowering 所需表，保证 codegen 行为不退化。
AST 主路径也已按 P0-Perf-4 收口：driver 持有 parser/module AST 并把 mutable 引用传给 sema 和 IR lowering，
`SemanticAnalyzer(const AstModule&)` 被删除以避免隐式整树复制，`CheckedModule::normalized_ast` 已改成轻量
normalization overlay，彻底不拥有 `AstModule` snapshot，sema 构造期不再对 `exprs/types` 做 `size+4096`
reserve，postfix suffix 创建不再按值复制胖 `ExprNode` / `TypeNode`。节点级 C symbol side table 也已从
`std::vector<std::string>` 改为 `expr_c_name_ids` / `pattern_c_name_ids` / `item_c_name_ids` 的 `IdentId`
表，实际文本只进入 checked module 的 C-name interner 去重，不再每个节点分配一个 `std::string`。
2026-05-15 的 compact AST 主线已继续落地：`TypeNode`、`ExprNode`、
`PatternNode`、`StmtNode` 和 `ItemNode` 的主存储从胖节点 vector 改成 32B compact header +
per-kind payload arena，module loader 合并模块时按 compact payload remap/append，不再依赖
fat-vector 地址或 `.data()` 指针反推 ID；sema 的 item owner 查询也改为显式 `ItemId`，不再靠
`items.data()` 地址反推。2026-05-16 继续把 sema、IR lowering 和 AST dump 的 `ExprNode` 热路径迁到 compact
view / payload 直读，移除 `ExprNode` 兼容 wrapper 和 literal 分析里的胖节点重建；同日继续落地
global bump allocator + syntax 层 `IdentifierInterner`，AST 的 type/expr/pattern/stmt/item/module/import
名字字段携带原生 `IdentId`，parser、module loader、postfix suffix 创建和 sema 入口都会把节点/metadata
重新收口到当前 `AstModule` 的 identifier arena，sema typed lookup key 不再维护第二套私有 interner；函数、类型、值、
generic template、enum case、struct field、method/member 和局部 scope lookup 都使用 `IdentId` typed 索引，
checked-module map 不再保留并行 string-key lookup 路径；生成的 ABI/display/dump 文本进入 bump-backed
`IdentifierInterner`，`FunctionSignature`、`Symbol`、`StructInfo`、`EnumCaseInfo`、`TypeAliasInfo` 和 `TypeInfo`
只保存 `InternedText` / typed id，不再持有堆分配 `std::string` payload。2026-05-16
性能线随后删除旧胖 `ExprNode` 生产类型，parser 创建、`AstModule` 存储、module loader append 和 postfix
suffix 都直接写 compact expression header + per-kind payload。随后又把 parser 表达式创建 API
收紧到字段级 append/set：name、unary/binary、call、if/block/match、array、field/index/slice、
struct literal、generic apply、try 和 cast-like 节点都不再先构造 payload 大临时对象；postfix suffix
的 call/field/index/slice/generic apply/struct literal/try 路径也直接写 compact payload。2026-05-16 后续 bump pass 又把
`TypeNodeList` / `ExprNodeList` / `PatternNodeList` / `StmtNodeList` / `ItemNodeList` 的 header vector 和
per-kind payload vector 接到 `BumpAllocatorAdapter`，`IdentifierInterner` 的 text vector、hash bucket/node
也进入同一个 bump arena；parser 会根据 token 形态估算 statement/item/type/pattern/identifier 规模，并在 AST
模块创建初期按 token 形态对 expression header 和每类 payload 计算容量上界，直接从 bump arena 取好 vector backing storage 并对表达式 arena page pre-touch；parser 创建表达式节点时只在这些已分配区间内顺序 emplace，不再依赖 vector 热路径自动扩容，也避免解析过程中逐节点首次触达新页。后续 lexer/sema bump pass 已把 lexer token 输出改为 `TokenBuffer`，token vector backing storage 由 bump arena 持有并一次性 reserve；token 容量不再被 262144 上限截断，也不再预触永远不会写入的估算余量页，避免 bump vector 扩容留下旧 backing storage。sema 的 `CheckedModule`、`GenericSideTables`、`PatternCaseNameTable`、`TypeTable`、`SymbolTable`、analyzer lookup/cache 主表、`FunctionSignature` 参数/generic args、`StructInfo` 字段、`EnumCaseInfo` payload 列表、`TypeInfo` tuple/function/generic args、generic template 参数列表和 generic constraint bucket 也改为 bump-backed storage；sema 持久 name / c_name / value_text / generic key 字段改为 `InternedText`，复制 `CheckedModule` / `TypeTable` / `SymbolTable` 时会重新 intern 到目标 arena，不再复制字符串 buffer。generic function instance 列表使用 bump-backed deque 保持元素地址稳定；retained generic instance 使用函数体局部 NodeSpan side table，只有非连续节点 ID 映射才共享 module-level sparse layout，不再按实例复制 sparse ID mapping vector；generic method / enum-case / visible-module cache bucket 不再由 `operator[]` 创建普通 heap vector；IR lowering 的源码 local lookup 和 IR verifier 的 symbol 去重也改为 interned typed id，不再保留持久 string-key map。
2026-05-16 后续补齐了 source-name 去重：`FunctionSignature.name`、`Symbol.name`、`StructInfo.name`、
`StructFieldInfo.name`、`TypeAliasInfo.name`、`EnumCaseInfo.name/case_name/enum_name`
等源代码已有 identifier 的字段在普通 driver 路径直接借用 `AstModule::identifiers`，不再把函数名、字段名、
局部名再复制进 checked C-name interner；`CheckedModule` move 只重绑原本属于自身 `c_names` 的
`InternedText`，不会把 borrowed AST name 错绑到 checked interner，显式 copy 才重新 intern 到目标模块。
ABI 校验也从临时 `IdentifierInterner` 改为 `std::string_view` key，避免校验阶段把所有 C symbol 再复制一遍。
benchmark 和 stress profile 是机器相关 RSS/耗时数据的唯一来源；主文档只保留 gate 形态和 profile/scale 校准机制，
不再把本机样例数字写成可移植 baseline。
generic side table 生命周期已在主路径收口：sema-only expected-type 和 pattern-case cache 进入可释放 arena 并在分析后丢弃；retained instance 只保留 lowering 需要的表；非连续 NodeSpan sparse ID mapping 按模板共享；小型 per-instance side table arena 从默认 64 KiB floor 降到 1 KiB block，覆盖 2000/5000+ 不同泛型模板的固定开销。跨模块 stable hash、parallel global ID、轻量 generic/AST/diagnostic 阈值门、默认 Release+LTO 的 5000 generic / 2M 高复杂 AST / 5000 errors 发布阈值门、8 GiB AST RSS 阈值、阶段级 profile，以及 profile/scale 形式的跨机器阈值校准机制已接入；后续只保留新增机器 profile 数据和 query 级增量复用，不再缺身份或 release gate 主路径。
2026-05-17 M2.5 起步后，诊断协议先做了结构化收口：语义诊断现在由显式 semantic kind 映射到稳定 category/code，message 只保留展示职责；CLI 文本、JSON 输出和后续 diagnostics query / LSP 适配将共享同一事件语义。随后 M2.5 主线完成第一批 query key 与依赖跟踪，下一阶段转向 lossless syntax 和 IDE-native 入口，而不是继续扩张语言面。
2026-05-21 M2.5 收尾重构继续推进：`src/driver/diagnostic_renderer.cpp` 已经是唯一的诊断输出层，
`src/driver/module_loader_remap.cpp` 已把 AST remap / append 从 `module_loader.cpp` 中拆出；
incremental cache 也已经从单个巨型 `incremental_cache.cpp` 拆成 `src/driver/incremental_cache/`
内部模块，分别承担 format、io、query orchestration、subjects、reuse、source-stage reuse、
schedule、profile 和 query stats。`subjects` 又继续拆出 source、module、semantic、ordering
四个内部域；随后按聚合度要求把过细的 incremental cache `io/fingerprint.cpp`、`io/reader.cpp`
和 `io/writer.cpp` 回收进单个 `io.cpp`，让路径规范化、source fingerprint、cache parser /
validator、row writer 和原子发布共享同一个 wire-format 权威入口。`module_loader.cpp` 现在只做文件读取、import 解析、模块校验和递归编排；
`incremental_cache.cpp` 则降为 public facade，不再持有协议、I/O、query subject 生成、reuse plan
或 profile 明细。driver / backend public header 也已开始按 facade 边界瘦身：
`EmitKind`、`DiagnosticOutputFormat`、`ModuleRecord`、`LlvmIrEmitter` 和
`OptimizationLevel` 拆为窄接口头；`compiler.hpp` 不再包含完整 `ir.hpp`，
`incremental_cache.hpp` 不再把 module loader、checked module 和 AST 头传递给所有调用者，
`diagnostic_renderer.hpp` / `native_toolchain.hpp` 也只依赖各自需要的 enum。query 层新增
`query_graph.hpp` / `query_graph.cpp`，让 graph node / edge / dependency-kind 规则从
provider-heavy 的 `query_context.hpp` 中分离出来，`query_edge_verifier.hpp`、`query_replay.hpp`
和 `query_reuse.hpp` 只在实现文件里回到 context。随后 query 层继续新增
`query_provider_set.hpp` / `query_provider_set.cpp`，把 provider 类型别名、默认 provider wiring
和自定义 provider 回退策略收口为独立 Strategy 集合，`QueryContext` 不再直接持有十五个
provider 成员，而是专注 query 生命周期、依赖图维护、缓存/失效协调和 provider set 转发；
后续又把 `QueryProviderSet` / `QueryContext` 的 15 参数 provider 构造入口收束为
`QueryProviderOverrides` 聚合对象，短构造和 setter 继续保留，但新增或扩展 query provider
不再需要把所有 provider 作为并列参数穿透 public facade；
`query_executor.hpp` 也改为前置声明 `QueryContext`，避免 batch request API 被动包含完整
context 实现。
module loader 继续收口但保持聚合度：`module_loader_support.cpp` 作为私有 support 聚合点，
承接路径/canonicalization、import 候选、文件读取、lex/parse、模块诊断和身份校验；
`module_loader_remap.cpp` 继续只负责 AST remap / append 独立算法域；`module_loader.cpp`
保留加载状态机、模块登记、import 递归和 append 编排。加载中集合清理改为局部 RAII scope，
错误分支不再手动重复 erase。后续新增或重构函数参数数以 6 个为上限，超过时必须用具名
context/value object 或重新切分职责，避免低聚合度 helper 继续扩散。parser 的
`parser_source_ranges.cpp` 也已并回 `parser_part_ranges.cpp`，让 source-range composition 和
AST range lookup helpers 留在同一个 range reader 聚合点。
2026-05-22 继续把 parser postfix 的 `[]` 判定收成中等粒度分类层：
新增 `src/parse/bracket_suffix_classifier.hpp` / `src/parse/bracket_suffix_classifier.cpp`，
统一拥有空 `[]`、slice、type-only 参数、generic call/literal continuation、selector continuation
和嵌套 generic continuation 的分类规则；`parser_postfix.cpp` 仍保留参数解析、错误恢复和 AST
节点构造，不把 index/slice/generic apply 继续拆成低聚合度小文件。
2026-05-22 sema 开始按“外部不散，内部不耦合”的路线做第一阶段 facade/core 收口：
`include/aurex/frontend/sema/sema.hpp` 已从约 800 行瘦到 39 行，只保留 `SemanticOptions`
和 `SemanticAnalyzer` 稳定入口；旧 analyzer 的内部类型、lookup cache、泛型模板信息、
表达式 view、pattern/statement helper 和 analyzer 状态先移动到 `src/sema/internal/sema_core.hpp`，
由 `src/sema/sema_facade.cpp` 通过私有 `Impl` 持有。生产调用方继续只依赖 public facade，
sema 实现和内部测试才显式包含 `<sema/internal/sema_core.hpp>`；测试不再通过预处理器开关打开生产类型的私有区。
这一步只建立稳定外部入口和内部 core 边界，不改变 sema 算法；后续在 `SemaState`
稳定后再引入 NameResolver / TypeResolver / GenericEngine / FunctionChecker /
ExprChecker / StmtChecker / PatternChecker 等中等粒度 domain service。同步把
`FunctionBodyContextScope` 的 7 参数构造器收束为具名 `Config`，继续执行新增或重构函数参数不超过
6 个的规则。新增 public facade 测试覆盖 borrowed AST 和 owned AST 两个入口，`src/sema/sema_facade.cpp`
lines/functions/regions 均达到 100.00%。
同日继续完成第一块状态 owner 聚合：`current_module`、当前函数返回类型/推导、当前泛型上下文、
当前泛型 side table、loop depth、unsafe depth 和 const initializer 标记收进
`SemanticAnalyzerCore::FlowState`，旧的散落 mutable 字段不再直接挂在 analyzer core 上。
这一步仍然不新增深层目录，也不拆算法文件，只先把“当前分析上下文/控制流状态”确认为唯一 owner；
Name/Type/Generic/Function/Module 等更大的 owner 随后继续收进状态层。
随后继续把剩余 sema mutable table 收进中等粒度 owner：`NameState` 持有本地 symbol table、
typed lookup index、函数/值/enum-case 名称索引；`TypeState` 持有 named type、type alias
resolution stack/cache 和 struct side table；`GenericState` 持有泛型模板表、实例缓存、
param query key 和 placeholder function；`FunctionState` 持有 global value、definition item
和 body state；`ModuleState` 持有 visible/export module cache。构造期也改为按这些 owner
接入 bump arena，不再在 analyzer 构造器上平铺几十个 map 初始化。算法文件暂不继续切碎，
下一阶段才引入 NameResolver / TypeResolver / GenericEngine 等 domain service。
随后把 `CheckedModule`、bump arena 和这组 owner 统一包进 `SemaState`，`SemanticAnalyzerCore`
现在只直接持有 `state_` 一个可变语义状态入口；生产代码和白盒测试都通过 `state_.checked`、
`state_.names`、`state_.types`、`state_.generics`、`state_.functions`、`state_.modules`
和 `state_.flow` 访问对应 owner，避免 analyzer core 继续暴露一排平铺 mutable table。
随后补齐 context 层：`SemaContext` 统一持有 AST module、diagnostic sink 和 semantic options，
`SemanticAnalyzerCore` 不再直接平铺 `module_`、`diagnostics_`、`options_` 三个外部会话字段。
这让后续中等粒度 service 可以拿 `SemaContext&` 加必要 state owner，而不是继续依赖完整 analyzer。
`SemanticAnalyzerCore::analyze()` 也收成 phase pipeline：prepare、reserve、declaration、body、
validation 和 finish 六个阶段方法留在 `sema.cpp` 聚合点内，只表达阶段顺序，不为每个小阶段新建
目录或文件。
同轮把两个历史长参数接口继续收口：函数注册改为 `FunctionRegistrationRequest`，
generic side table 本地布局改为 `GenericSideTableLocalLayoutView`，不再让 item/owner/key/type/
incremental identity 或 expr/pattern/type/stmt sparse node id 以 8 到 10 个并列参数在调用链上传递。
随后 sema 的第一块中等粒度 domain service 也已落地：新增
`src/sema/internal/name_resolution.hpp` / `src/sema/internal/name_resolution.cpp`，由
`ModuleVisibilityResolver` 统一承接 import alias 解析、visible/export module cache、
模块路径匹配、public re-export 遍历和 module display name。`SemanticAnalyzerCore` 保留原有
方法作为薄 facade，`sema_lookup.cpp` 继续聚合 typed lookup 和 did-you-mean 逻辑，没有把
表达式或 lookup case 拆成碎片文件；resolver 只通过 `ModuleState` 的 mutable cache 写入可见性结果，
public header 仍只暴露稳定 sema facade。
相关 cache/query/incremental gtest、完整 ctest、改动 C++ 文件格式检查和
`tools/check_coverage.sh` 已经重新跑过，source totals lines/functions/regions 覆盖率分别为
95.27% / 97.79% / 95.20%，新的 `src/sema/internal/name_resolution.cpp`
lines/functions/regions 为 100.00% / 100.00% / 97.47%，新的聚合
`src/driver/incremental_cache/io.cpp` lines/functions/regions 为 95.30% / 100.00% / 96.11%，`module_loader_support.cpp`
三项为 99.38% / 100.00% / 98.65%，`aurexc` entrypoint 三项为 100.00%。
M2.5 第一批 query-key 主路径已经闭环：`--incremental-cache` 默认使用 query-key pruning，
只有显式 `--no-query-pruning` 才退回 coarse source-fingerprint 兼容路径。当前 query
基础设施覆盖 stable key、canonical type / generic instance identity、`QueryContext`
row/edge 落盘与 replay、source-stage green reuse、red-green provider-skip、profile
事件、query graph fuzz、sanitizer 和 release/coverage 门禁；后续 lossless syntax、
IDE-native 入口和高级语言特性都必须复用这条主路径。
lossless syntax 专项已经完成当前 M2.5 验收边界：lexer 增加 opt-in trivia token emission，默认
编译路径仍跳过 trivia；`LosslessSyntaxTree` 保存完整 token 序列，提供结构化
node/element/token-leaf API，记录 parent 和连续 token span，能校验 tree invariant，支持按 offset
查最深节点，并能重建整文件或任意 node 子树源码。dump 形态已经从原始 token stream 前进为：
`source_file` 下挂 `module_decl`、`import_decl`、`function_decl` 等顶层声明节点、直接 trivia/eof
token leaves，以及 `block` / `paren_group` / `bracket_group` / `brace_group` 分隔符组节点；
`token_stream` 只保留为非单调手造 token span 的保守兜底。parser 层新增 lossless CST -> AST
lowering façade，过滤 trivia 后走现有 parser，并用 AST dump parity 覆盖正常 semantic token 路径。
query-key 侧已经确保 retain-trivia 的 `LexFileKey` fingerprint 使用 trivia lexer，build-lossless
parse result fingerprint 混入 CST 结构，`lossless_tooling` 的 parse provider 依赖对应 retain-trivia
lex query。完整局部重解析仍属于后续更深优化，不再是 lossless syntax 基线缺口。
IDE-native 工程入口已经完成当前验收：新增 `aurex_tooling` 目标和
`include/aurex/application/tooling/ide.hpp`。`IdeSnapshot` 面向内存 buffer，一次构建统一产出
source manager、lossless tree、AST、checked module、结构化 diagnostics，以及
file/lex/parse/diagnostics query records 和 dependency edges；offset token、hover、顶层
definition、同名 identifier references、checked-backed 的全局查找、AST fallback 的局部参数 /
`let` 绑定查询，以及编辑影响 node 选择都通过这层 API 暴露。diagnostics 在 CLI 渲染和
query fingerprint 之前会先转成结构化 event stream。该入口不绑定 LSP protocol，后续 LSP
adapter 只消费 snapshot 数据，不再旁路 parser/sema/query 主路径。
2026-05-17 正则性能/测试线继续把 `RegexSet` exact-literal prefix trie 推进为持久标量 Aho-Corasick fast path：纯字面量 set 构建共享 trie 后补 failure/output link，`matches_set`、`find_set`、first-match scan、all-span/overlap scan 和 vectored flatten 后入口都用同一份自动机线性扫描；database 升级为 v3，序列化 node/terminal/max-literal 元数据，roundtrip 后不退回 VM active-list。测试侧补上 Unicode byte span、suffix failure output、重复 literal、database fast path workspace 和 deterministic RegexSet property corpus；`tools/regex_differential.py` 现在同时生成固定 + property Python `re` 差分、RegexSet exact-literal property cases、Unicode 17.0 full case-fold 与 UAX #29 `\X` conformance 程序，并作为 opt-in CTest slow conformance 入口，需通过 `-DAUREX_ENABLE_REGEX_CONFORMANCE=ON` 显式注册。
2026-05-16 后续表达式 P0 语义线把 expression type cache 从 final-only 记录拆为三层：
`expr_intrinsic_types` 保存表达式自身类型，`expr_types` 继续保存当前语义使用的 contextual final type，
`expr_expected_types` 作为 final cache key，`CoercionRecord` 记录 contextual integer/float literal、`null`
到 pointer、slice mutability 等调整。主模块和 generic side table 都有对应 intrinsic/final 存储，local dense
和 sparse fallback 行为一致；integer/float/null、unary/binary、slice、array/tuple literal、if/block/match
在 expected type 下会保留 intrinsic type，不再把 expected type 污染成表达式自身类型。IR lowering 仍读取
`expr_types` final table，coercion/intrinsic 只作为 checked 语义 overlay。
同日后续工程质量线把 `analyze_expr` 从单一大 dispatch 收口为一条明确主路径：
`analyze_expr(expr, expected)` 只负责 final cache lookup / expected key 记录，`analyze_expr(expr, view, expected)`
只做表达式类别调度；literal、value/name/call、control、aggregate、projection、operator、builtin 分别进入小型
helper。二元表达式内部也拆成 operand contextual typing、类型不匹配诊断、整数字面量 hazard 检查和 operator
result 记录，没有保留并行的新旧 analyzer 路径。
2026-05-16 后续 match 性能/正确性线又把结构化穷尽检查从“枚举 bool / enum 叶子笛卡尔积”替换为
pattern matrix / usefulness witness search：bool、enum payload、tuple、struct、4096 列显式 M2.1 边界内的 fixed array、open integer literal
和 dynamic slice 通过 constructor specialization、default matrix 和 slice 代表长度特化判定覆盖和 unreachable arm；超过 4096 元素的 fixed array 不再隐式穿透实现上限，而是要求不可反驳 arm；
无 guard 和字面量 true guard 计入覆盖，字面量 false 和动态 guard 不计入覆盖。动态 slice 不再只能靠 `[..]`
兜底，`[]` + `[_, ..]`、bool head partitions 等有限代表长度覆盖可被证明；开放整数域 literal 也进入
usefulness constructor，重复 literal arm 会被判为 unreachable，缺少剩余整数域时输出 open-domain wildcard 诊断。

当前 `build` 目录可能不是完整测试配置；可信验证应以 `tools/run_tests.sh` 重新 configure/build/ctest 为准。

## M2 当前短板

M2 的核心短板集中在语言地基，不在标准库规模：

- block statement 和 block expression 主体规则已统一；expression block 可完整承载普通 statement，并额外要求 final expression。
- const initializer 已补齐纯标量运算；当前仍没有函数调用、控制流表达式或完整 comptime。
- compound assignment 已补齐；`++` / `--` 自增自减语法已从 M2 基础语法移除，统一使用 `+= 1` / `-= 1`。
- 基础 range-for 已补齐为 `for i in range(end)` / `for i in range(start, end)` / `for i in range(start, end, step)`；当前仍没有容器迭代、slice 迭代或 iterator protocol。
- trailing separator 策略已冻结：圆括号/方括号列表允许 trailing comma，comma 分隔花括号列表允许但不强制最后一个 comma。
- 现代基础语法继续收口：ADT-first enum、enum multi-payload destructuring、array literal / repeat literal、slice type/expression、tuple/destructuring、function type / function pointer 和基础字面量体系已完成。
- default private 已完成：顶层 item、struct field、impl method 和 import 默认 private，跨模块 API 必须显式 `pub`；`export c fn` 仍强制 public。
- `pub fn` 返回类型已收紧为必须显式；private helper 仍可推导。
- lexer 已支持嵌套 `/* ... */` 块注释。
- enum 已支持 ADT-first 形态：普通 enum 可省略 base type 和 discriminant，tag 自动分配；多字段 payload 可用 `.case(a, b)` pattern 按字段解构；显式 `enum Status: u8 { ok = 0, err = 1 }` 仍作为 C-like/repr enum 形态保留。generic enum 已进入 M2 基线，`Option[T]` / `Result[T, E]` 这类类型参数 ADT 可被实例化和匹配。
- 数组、slice、tuple、函数指针和字面量基础语法已闭合：固定数组支持 `[1, 2, 3]` 和 `[0; 128]`，borrowed slice 支持 `[]const T` / `[]mut T` 以及 `a[l:r]`、`a[:r]`、`a[l:]`、`a[:]`；tuple 支持 `(A, B)` / `(A,)` 类型、`(a, b)` / `(a,)` 字面量、局部 `let (a, _) = value;` 解构、tuple pattern 和数字字段访问 `value.0` / `value . 1`；匿名 tuple 不支持 `.first` / `.second` 这类命名字段；函数类型支持 `fn(i32) -> i32`、`extern c fn(*const u8, ...) -> i32`、函数名作为值、局部/参数/字段函数指针间接调用；字面量支持普通字符串、C 字符串、raw/multiline raw string、byte string、byte literal、Unicode scalar `char` 和整数/浮点类型后缀。
- 最小 `unsafe` 已落地：`unsafe { ... }` 可作为 statement 或 expression，`unsafe fn` 和 unsafe 函数指针类型会把调用限制在 unsafe context 内；raw pointer 解引用、`ptrcast`、`bitcast`、`ptrat`、`strraw` 都必须在 unsafe context 中使用。
- 泛型已支持最小非资源类 `where` capability predicate：`Sized`、`Eq`、`Ord`、`Hash`。`Copy` / `Drop` 等资源类约束、用户自定义 trait、associated type、const generic 和 trait object 仍暂缓。
- 泛型边界已扩展到 generic struct / function / enum / type alias，以及 `impl[T] Box[T]` 这类 owner generic impl；method-local generic parameter 仍不属于 M2。
- M1 的语言级 `noncopy` / `move` MVP 已从 M2 基线删除。当前先保留普通值语义和必要的数组/含数组类型限制；copy/drop/borrow/ownership 暂缓为后续资源语义专题。
- 最小 safe reference 已落地：`&T` / `&mut T`、`&place` / `&mut place`、reference 安全解引用、`&mut` 可写 place 检查、`&mut T` 到 `&T` 的只读化赋值，以及按 pointer-sized ABI lowering。raw pointer 解引用仍必须在 `unsafe` 中；borrow checker、lifetime、borrowed return、alias/resource 语义继续暂缓。
- `str` 已有语言级雏形，普通数组/slice 地基已落地，`strraw` 已纳入 unsafe；M2 no-std checked UTF-8 边界已冻结为 `strvalid(bytes) -> bool`、`strfromutf8(bytes) -> str` 和 `text[l:r]` checked slicing。`strfromutf8` 失败或 `str` 切片越界/落在 UTF-8 continuation byte 上时返回空 `str`，不会把无效输入包装成 `str`。更完整的 text API、Unicode scalar/grapheme 迭代和拥有型 `String` 仍后置到库层重建。

当前完整语法库存、已支持高级能力、未完成特性和基础语法优先级见 [Aurex 当前语法与特性清单](language-feature-inventory.md)。

## 当前结论

M2 的正确目标是先把基础语法和核心语义做窄做稳，再谈标准库、自举和构建工具。当前编译器已经能支撑语言核心实验和 native 输出，但不应把 M1 的 std/selfhost 经验继续当作有效路线推进。

下一步最重要的是继续冻结 M2 语法基线。ADT-first enum、generic enum、enum multi-payload destructuring、数组值语法、slice type/expression、tuple/destructuring、function pointer / function type、字面量体系、最小 unsafe 边界、最小 safe reference、M2 pattern ergonomics、最小非资源类 `where` capability 和 `str` checked UTF-8 边界已经落地；pattern 当前支持 tuple match pattern、slice pattern、struct pattern、nested enum payload destructuring、局部 struct/slice/enum destructuring、binding or-pattern alternatives、`let ... else`、`if value is pattern` / `while value is pattern`，以及 if 表达式 pattern condition。结构化 match 穷尽检查已使用 pattern matrix / usefulness witness search 覆盖 bool、enum payload、tuple、struct、fixed array、open integer literal 和 dynamic slice，不再枚举笛卡尔积；guard 已区分无 guard、字面量 true/false 和动态表达式，动态 slice 长度和开放域 witness 已进入当前 M2.1 主线。
