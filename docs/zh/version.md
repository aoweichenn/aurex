# 版本文档

## M8 Dyn Trait、Erased View 与动态派发设计基线

M8 主线已从最新 M7 基线开出，M8a-M8e 已完成 dyn trait / erased view 调研设计、query 地基修正、frontend
syntax/sema、borrowed dyn coercion、checked vtable facts、IR/backend runtime dispatch 和 hardening closure。
M8 不把 dyn trait 当成“照抄 Rust trait object”或
“补一个 parser 分支”；新的设计基线选择 Aurex 自己的 origin-bound erased view：第一版只做 borrowed dyn view，
复用 M7 origin / loan / lifetime facts，以 checked vtable witness 描述动态派发。

当前新增实现包括：

- 新增 [Aurex M8 Dyn Trait、Erased View 与动态派发设计基线](m8-dyn-trait-design.md)，整理 Rust、Swift、
  Go、C++ ABI 的取舍，并固定 Aurex 的非照抄路线。
- 移除 query 层无语义形状的 `CanonicalTypeKind::trait_object` 占位。该旧设计没有 principal trait、
  associated equality、object origin/lifetime 或 vtable layout identity，却被 decoder 和 tests 当作有效
  canonical type key；M8 后续不能在这个错误稳定 key 上继续扩展。
- 新增结构化 M8a query identity：`TraitObjectTypeKey`、`VTableLayoutKey`、`TraitObjectCoercionKey`。三者分别表达
  borrowed erased view 类型、checked vtable witness 和 borrow-to-dyn coercion，不再把 canonical type、vtable
  layout、conformance evidence 混在一个 kind tag 或 children list 里。
- 更新 stable key decoder 与 query tests：当前 canonical type key 只承认可由现有语言/语义实际产生的类型形状。
  M8a decoder 还会验证 trait object / vtable / coercion key 的 schema、policy、principal trait、associated
  type member 和嵌套 canonical type 形状，并拒绝三类 key 布局混用。
- 新增 M8b frontend surface：`dyn Trait`、qualified dyn trait、trait args 和 associated equality 可 parse、
  AST dump 和 sema resolve；bare `dyn Trait` 仍不能作为普通 storage type，只允许经 `&dyn Trait` /
  `&mut dyn Trait` 等 reference pointee 使用。object-callability 会诊断缺少 self receiver、非法 receiver、
  缺失/未知/重复 associated equality 和 unconstrained `Self`。
- 新增 M8c borrowed dyn coercion：`&T -> &dyn Trait`、`&mut T -> &mut dyn Trait` 会检查可见 nominal impl 与
  associated equality，成功时记录 checked vtable layout、method slot、callability 和 coercion facts；dyn receiver
  method call 绑定为 `TraitMethodDispatchKind::vtable_slot`。
- 新增 M8d IR/backend runtime dispatch：IR 显式表达 `trait_object_pack`、`trait_object_data`、
  `trait_object_vtable` 和 `vtable_slot`；verifier 检查 vtable layout、slot schema 和 erased receiver ABI；
  LLVM backend 生成 `{data*, vtable*}` fat view、internal vtable global 和 indirect call。
- 新增 M8e hardening：default method slot、associated equality dispatch、checked/IR fingerprint、lower-IR unit
  invalidation、native execution 和文档收口都已补强。
- 更新 `next-steps` 与中文文档入口，把 M8a-M8e 路线明确为：query 地基、syntax/sema、borrowed dyn coercion、
  IR/backend dispatch、hardening/后续扩展评估。

当前仍保守的边界：M8 只完成 borrowed dyn view 和 runtime method dispatch，不实现 owning dyn、
`Box<dyn Trait>`、allocator、标准库、dynamic Drop dispatch、supertrait upcasting 或多 trait object composition。

## M7d-K Array Repeat Resource Safety Closure

M7d-K 已完成 compiler-only array repeat resource safety 收口。本阶段继续不实现标准库，也不引入 `Clone`、
owned resource wrapper、generic Drop surface 或任何库级资源 API；新增能力只发生在 Sema array repeat
规则、ownership/borrow/place-state 运行期遍历语义、IR lowering 防御和测试覆盖内部。核心语义是：`[expr; N]`
不能在没有复制/克隆构造语义时隐式复制非 `Copy` 资源。

当前新增实现包括：

- Sema 对 repeat array literal 增加资源规则：`[expr; 0]` 与 `[expr; 1]` 不要求 `expr` 的元素类型是
  `Copy`；`[expr; N]` 且 `N > 1` 时，元素类型必须满足 compiler-owned `Copy` capability，否则诊断
  `array repeat value must be Copy when repeated more than once`。
- `[expr; 0]` 仍会按元素类型上下文对 `expr` 做类型检查，避免无效表达式从零长度 repeat 中漏过；但运行期
  ownership/move/borrow/place-state/flow traversal 不把 repeat value 当作会求值或会 consume 的表达式。
- `[expr; 1]` 按一次普通 owned value transfer 处理，允许非 `Copy` 资源进入长度为 1 的数组。
- move analysis、place-state precheck、body-flow return scan、loan checker origin traversal、borrow summary 和
  storage escape origin traversal 都使用同一个 checked array repeat runtime helper，避免不同分析阶段对
  zero-repeat value 的运行期语义分叉。
- IR lowering 对 repeat literal 保持防御：长度为 0 时只生成空 aggregate；若非法的非 `Copy` 多元素 repeat
  绕过 Sema 到达 lowering，只生成空 aggregate placeholder，不复制同一个 owned `ValueId`。
- 覆盖新增 positive samples：单元素非 `Copy` 资源 repeat、零长度非 `Copy` 资源 repeat；新增 negative sample：
  非 `Copy` 资源多元素 repeat。白盒测试覆盖 Sema copy 需求、零长度运行期跳过、IR lowering 防御和 sample
  suite 诊断。

当前能做的事情：

- `let files: [0]File = [make_file(); 0];` 可以通过：`make_file()` 类型检查为 `File`，但运行期不被求值，
  不产生 move/borrow/cleanup 消费。
- `let files: [1]File = [make_file(); 1];` 可以通过：`make_file()` 被求值一次，数组持有一个 `File`。
- `let values: [4]i32 = [0; 4];` 继续按 `Copy` 标量 repeat 生成数组值。
- `let files: [2]File = [make_file(); 2];` 会在 Sema 阶段拒绝，避免在没有 Clone/fill constructor/rollback
  构造协议时隐式复制同一个资源值。

当前仍保守的边界：本阶段不实现标准库 API，不实现 `Clone`、array fill constructor、非 `Copy` 多元素 repeat
构造、用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、dynamic destructor ABI、
async/unwind-aware drop 或 panic cleanup ABI；也不实现 consuming pattern payload transfer、non-`Copy` `?`
payload transfer、indexed move-out、array/slice/index 精确 disjoint proof 或 replace/take/swap primitive。

## M7d-J Cleanup Marker Query / Tooling Consumption Closure

M7d-J 已完成 compiler-only cleanup marker query / tooling 消费面收口。本阶段继续不实现标准库，也不引入任何
库级 owned resource wrapper；新增能力只发生在 IR cleanup marker facts、query/cache result、driver incremental
cache subject、IDE/tooling semantic fact 和测试覆盖内部。语言行为不放宽：generic / associated / opaque /
unknown cleanup 仍保持 marker-only，consuming pattern payload、non-`Copy` `?` payload transfer 和 indexed
move-out 仍按当前规则拒绝。

当前新增实现包括：

- query 层新增稳定 DTO：`CleanupMarkerKind`、`CleanupMarkerPolicy`、`CleanupMarkerFact`、
  `CleanupMarkerSummary` 和 `FunctionCleanupMarkerFacts`。这些 facts 有稳定 fingerprint、summary 和 dump，
  工具链可以消费 facts 而不依赖 IR 内部结构。
- IR 层新增 cleanup marker facts 提取器：`function_cleanup_marker_facts(...)`、
  `function_cleanup_marker_facts_by_symbol(...)` 和 `query_cleanup_marker_policy(...)`。提取器按函数 value
  closure 收集 `drop` / `drop_if` marker，记录 value id、object、condition、target type 和 cleanup ABI policy。
- `lower_function_ir` / generic lower IR query 的 provider input/output 现在携带 `FunctionCleanupMarkerFacts`；
  query result fingerprint 使用 raw lowered IR fingerprint 加 cleanup marker facts fingerprint 和 summary count
  共同生成，避免 cleanup ABI 事实变化被 cache 误复用。
- driver incremental cache 的 lower-IR subject 分离 raw IR input fingerprint、final query result fingerprint 和
  cleanup marker facts；调度 provider 时传入 facts，而不是把 final result 当 raw IR input 二次混入。
- tooling full build 下 `aurex_tooling` 可选择性链接 `aurex_ir` 并启用 `AUREX_TOOLING_ENABLE_IR_FACTS`，
  IDE snapshot 会在 sema 成功后临时 lower IR 并填充 `snapshot.cleanup_marker_facts`；frontend-only build 不链接
  IR target，facts 为空且行为保持兼容。
- IDE semantic facts 新增 `cleanup_marker_facts` kind，并把该 fact 关联到 `lower_function_ir` query；
  workspace index 和 session reuse invalidation 识别该 fact 为 body-local / indexable；函数 hover 在存在 facts
  时显示 `cleanup_markers=count=...`。
- 覆盖率补齐到新增代码自身：`cleanup_marker_facts.cpp` region 100%，`lower_function_ir_query.cpp` region 100%，
  `ir_cleanup_marker_facts.cpp` region 95.89%；全量 coverage gate 的 source lines/functions/regions 均达到 95% 门槛。

当前能做的事情：

- 编译器能把 M7d-G 之后 IR `drop` / `drop_if` marker 上的 cleanup ABI policy 稳定投影给 query/cache/IDE，
  不需要 IDE/LSP 重新扫描 IR dump 字符串或重新推导 lowering 行为。
- lower-IR query cache 能感知 cleanup marker policy、数量和分类变化；generic/opaque/associated/unknown marker-only
  事实改变时，query result 会正确失效。
- tooling full build 能在函数 hover、semantic facts、workspace index 和 reuse plan 中消费 cleanup marker facts；
  frontend-only build 仍保持轻量，不强制引入 IR 依赖。
- 这为后续 dynamic Drop ABI、generic cleanup runtime ABI 或 payload transfer 语义提供事实面，但当前不生成任何未知
  runtime destructor 调用。

当前仍保守的边界：本阶段不实现标准库 API，不实现标准库拥有型资源封装，不实现用户可写 `Drop` bound、generic
Drop impl、trait-object Drop dispatch、dynamic destructor ABI、async/unwind-aware drop 或 panic cleanup ABI；
也不实现 consuming pattern payload transfer、non-`Copy` `?` payload transfer、indexed move-out、array/slice/index
精确 disjoint proof、array repeat resource rollback 或 replace/take/swap primitive。

## M7d-I Move Rejection Facts Closure

M7d-I 已完成 compiler-only move rejection facts 收口。本阶段继续不实现标准库，也不引入任何库级 owned
resource wrapper；新增能力只发生在 sema move analysis、checked facts、query/cache authority、IDE/tooling
投影和白盒测试内部。语言行为不放宽：consuming pattern payload、non-`Copy` `?` payload transfer 和
indexed move-out 仍按当前规则拒绝。

当前新增实现包括：

- `CheckedModule::move_rejection_facts` 按函数记录当前 move analysis 实际发出的三类 unsupported 事实：
  `pattern_payload`、`try_payload` 和 `indexed_element`。事实包含关联 `expr`、`stmt`、`pattern`、当前用于资源判定的
  `tracked_type`、resource fingerprint、诊断是否已发出和 source range。
- move analysis 只在 reachable action 真实发出 unsupported diagnostic 时记录事实；不可达路径不会凭空产生
  checked fact，避免 query/tooling 与用户可见诊断不一致。
- `FunctionMoveRejectionFacts` 增加稳定 fingerprint、dump 和 checked-module copy/move 支持；checked dump 新增
  `move_rejection_facts` 段，便于后续调试 consuming pattern / try payload 的事实链。
- `TypeCheckBodyAuthority` 混入 move rejection fingerprint、总数、三类分类计数和 emitted-diagnostic 状态位；
  type-check body query result 会随这些 compiler facts 改变而失效。
- IDE semantic facts 新增 `move_rejection_facts` kind，workspace index / reuse invalidation 认识该 fact；
  函数 hover 在存在 facts 时显示 `move_rejections=count=.../first=...`。
- 新增 `move_rejection_facts_tests.cpp`，避免继续扩大既有资源/dropck 巨型测试文件；测试覆盖 match arm payload、
  struct pattern payload、if / if-expr / while condition payload、`?` payload 和 indexed move-out 三类拒绝事实。

当前能做的事情：

- 编译器能把“为什么当前拒绝这个 non-`Copy` pattern / `?` / index move”的事实稳定暴露给 checked dump、query
  authority 和 IDE/tooling，而不是只留下字符串诊断。
- 后续如果真正实现 payload transfer，可以用同一事实面验证从“拒绝”到“接受”的语义迁移，不需要 IDE/LSP 重新扫描
  AST 或重新推导 move analysis。
- 当前 diagnostic 与 fact 一致：只有已发出的 unsupported diagnostic 会进入 `move_rejection_facts`。

当前仍保守的边界：本阶段不实现 consuming pattern payload 的真实 move/reinit/drop 语义，不实现 non-`Copy`
`?` ok/some payload transfer，不实现 indexed move-out 或 array/slice/index 精确 per-element ownership proof；
标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、dynamic destructor
ABI、async/unwind-aware drop、panic cleanup ABI、array repeat resource rollback 和 replace/take/swap primitive
仍是后续独立工作。

## M7d-H Index / Slice Place-State Conservative Closure

M7d-H 已完成 compiler-only index / slice place-state 保守闭包。本阶段继续不实现标准库，也不引入任何库级
owned resource wrapper；新增能力只发生在 BodyFlow place identity、place-state facts、borrow checker 回归测试和
IR lowering cleanup place path 内部。

当前新增实现包括：

- place-state 的 semantic place identity 不再把本地 root 的 AST `ExprId` 当成不同 storage；同一个 local /
  parameter 的不同语法出现点会归并到同一个 root place。temporary root 仍保留 `root_expr`，避免不同临时值被误合并。
- field projection identity 只使用 `field_name_id`，tuple projection identity 只使用 `element_index`；index、slice
  和 dereference projection 不使用具体表达式 id 做精确区分，符合当前 M7d 对 array/slice/index 的 conservative
  may-alias 策略。
- place-state facts fingerprint schema 升级到 `sema.place_state.facts.v3`，避免旧缓存把 expr-id-sensitive facts
  当成新的 conservative facts 复用。
- BodyLoan checker 原有 projection conflict 规则继续保持：same/prefix 冲突、已知 struct field / tuple element
  disjoint 可放宽，index/slice/dereference/unknown projection 保守冲突；白盒测试显式覆盖不同 index expr 仍冲突。
- BodyFlow 的 return cleanup 链现在按已注册前缀构造：`return` 只清理执行到该点前已经进入作用域的 local/defer，
  return 后未执行的 local 声明和 defer 注册不会被误加入该 return path。
- IR lowering 的 `LocalPlaceProjectionKind` 增加 `index` 和 `slice`，`local_place_path` 现在能识别
  `local[index]`、`local[start:end]` 以及 `local[index].field` 这类本地 place path。cleanup prefix matching
  对 index/slice 只做同类保守匹配，不尝试证明不同下标或不同 slice range disjoint。

当前能做的事情：

- 本地 struct field 和 tuple element 的 partial move / reinit / cleanup drop flag 仍按 M7d-B/M7d-F 的精确规则工作。
- 本地 array/slice/index projection 在 borrow、place-state 和 lowering cleanup path 中不会被错误拆成互不相干的
  精确子 place；`a[i]` 与 `a[j]` 在当前阶段按 may-alias 处理。
- tracked resource 的 indexed move-out 仍由 move analysis 保守拒绝；resource index assignment 仍由 sema 报
  unsupported，避免在没有 per-element ownership/drop proof 时泄漏或双 drop。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、dynamic destructor ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern payload、array repeat resource rollback
和 replace/take/swap primitive 仍是后续独立工作。

## M7d-G Generic / Opaque Cleanup Marker ABI Closure

M7d-G 已完成 compiler-only cleanup marker ABI policy 正式化。本阶段继续不实现标准库，也不引入任何库级
owned resource wrapper；新增能力只发生在 sema drop-glue planner、IR cleanup marker、IR verifier、dump、
fingerprint 和 lowering 内部。

当前新增实现包括：

- sema drop-glue step 增加 `DropGlueAbiPolicy`，把 structural static cleanup、generic marker-only、
  associated-projection marker-only、opaque marker-only、unknown marker-only 和 static custom destructor
  明确区分。
- missing structural metadata 和 recursive drop-glue cycle 不再伪装成 `opaque_value`，而是记录为
  `unknown_value` + `unknown_marker_only`，避免后续 ABI 设计把未知结构误判为真正 opaque type。
- IR `drop` / `drop_if` marker 增加 `CleanupAbiPolicy` 字段；所有 lowering 生成的 cleanup marker 都会携带
  policy，IR dump 输出 `abi(...)`，IR fingerprint 混入 policy，clone/copy 会保留该事实。
- IR verifier 拒绝没有 cleanup ABI policy 的 `drop` / `drop_if`，拒绝非 drop value 携带 cleanup policy，
  并校验 marker policy 与 target type kind 匹配：generic、associated projection、opaque 和 structural/static
  marker 不再只能靠名字或注释区分。
- lowering 仍只为静态可解析 custom destructor 生成普通 direct `call`；generic、associated projection、opaque
  和 unknown cleanup 当前均保持 marker-only，不生成未知 runtime ABI 调用。
- LLVM backend 行为不变：`drop` / `drop_if` marker 本身仍是 no-op，真实析构副作用继续来自 M7d-D 已有的
  direct call lowering。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、dynamic destructor ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern payload、array repeat resource rollback
和 replace/take/swap primitive 仍是后续独立工作。

## M7d-F Tuple / Index Place-State Closure

M7d-F 已完成 compiler-only tuple numeric field 与 tuple element place-state closure。本阶段继续不实现标准库，
也不引入任何库级 owned resource wrapper；新增能力只发生在 parser、sema、borrow/place/dropck facts 和 IR
lowering 内部。

当前新增实现包括：

- parser 接受匿名 tuple 数字字段访问：`value.0` 和带空格的 `value . 0` 都会进入普通 field expression；
  `value.0f32` 仍保持为 suffixed float 边界，不被误当成 tuple field。
- sema 对 tuple field 做数字索引检查：`.0` / `.1` 等合法范围内字段得到对应元素类型，并作为可写 place
  参与 assignment；`.first` 这类非数字字段报 `tuple field access requires a numeric field`，
  越界数字字段报 `tuple field index is out of range`。
- BodyFlow place projection 增加 `tuple_element`，borrow conflict matrix 能区分同一 tuple root 下的不同已知
  tuple 元素；`pair.0` 和 `pair.1` 不再被当成同一个字段冲突。
- place-state、dropck 和 move analysis 能处理本地 owned tuple 元素的 partial move、reinit 和 cleanup facts；
  单元素 tuple `.0` consume 仍按 whole-object consume 处理，多元素 tuple 元素 move 作为 partial move。
- IR lowering 为 droppable tuple 元素建立元素级 cleanup/drop flag，tuple 元素 move-out 会关闭对应 flag，
  reinit 后重新打开，scope cleanup 只 drop 仍 initialized 的 tuple 元素；嵌套 tuple/struct droppable leaves
  使用同一套结构化 cleanup 展开。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、generic/opaque Drop ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、consuming pattern payload、array repeat resource rollback
和 replace/take/swap primitive 仍是后续独立工作。

## M7d-E Aggregate Rollback Codegen Closure

M7d-E 已完成 compiler-only aggregate rollback lowering 收口。本阶段不实现标准库，也不引入任何库级
owned resource wrapper；新增能力只发生在 IR lowering：当函数体中构造含 droppable 元素的 aggregate 时，
lowerer 会为部分初始化状态生成临时 storage 和 rollback cleanup action，保证后续元素求值提前终止时已经初始化
成功的元素会被清理。

当前新增实现包括：

- struct literal、tuple literal、array literal 和多 payload enum synthetic record payload 会在需要 rollback 时
  走 staged aggregate lowering。
- 每个 droppable 元素先完成表达式求值；只有当前 block 未被 `return` 等 terminator 关闭后，lowerer 才创建
  rollback drop flag、注册临时 cleanup action、store 元素值并把 flag 置为 initialized。
- 后续元素求值如果触发 early-exit，已有 cleanup stack 会发出 `drop_if` marker；M7d-D 已接入的 runtime
  drop lowering 会在 marker 旁生成静态 custom destructor direct call。
- 成功构造完整 aggregate 后，rollback cleanup action 会从当前 cleanup scope 中撤销，避免正常路径和后续
  local cleanup 重复 drop 临时元素。
- global/constant initializer、无 droppable 元素的 aggregate 和普通 scalar aggregate 仍保持 lightweight
  `ValueKind::aggregate` 路径，不引入临时 storage。

当前仍保守的边界：标准库拥有型资源封装、用户可写 `Drop` bound、generic Drop impl、trait-object Drop
dispatch、generic/opaque Drop ABI、async/unwind-aware drop、panic cleanup ABI、non-`Copy` `?` payload transfer、
indexed move-out、array/slice/index 精确 disjoint proof、array repeat resource rollback 和 replace/take/swap
primitive 仍是后续独立工作；tuple `.0` / `.1` source surface 与本地 tuple 元素 partial move 已由 M7d-F 补上。

## M7d-D RAII Runtime Lowering 与 Execution Closure

M7d-D 已完成 RAII runtime lowering 的第一版闭环。当前实现沿用 M7d-C 的窄 `Drop` surface 和
`CheckedModule::destructors` facts，在 IR lowering 阶段把可静态解析的 custom destructor 降低为普通
direct `call`，LLVM backend 通过现有 call emission 生成真实调用。

当前新增实现包括：

- cleanup lowering 解析 `DestructorInfo::function_key` 对应的 `FunctionSignature::c_name`，复用普通函数声明阶段
  建好的 IR symbol -> `FunctionId` 表，不再额外发明 destructor mangling。
- `drop(self: deinit T)` 按 by-value ABI 调用：lowerer 从 cleanup slot load 出 `T`，再把该值作为唯一实参传给
  destructor；不会把 slot 地址传给 `self: deinit T`。
- `drop` / `drop_if` 继续作为 target-independent cleanup marker 保留，便于 verifier、dump 和后续优化观察 cleanup；
  真实运行时副作用由 marker 旁路生成的 direct call 承担。
- 条件 cleanup 会读取 drop flag，并在独立 then block 中执行 custom destructor call；join 后清空 drop flag。
- 带 custom destructor 且含 droppable 字段的 struct local 会同时注册根 cleanup flag 和字段级 cleanup flag：
  scope exit、overwrite、early-exit 和 defer unwind cleanup 会先执行根 custom destructor，再按字段 cleanup
  反向处理仍 initialized 的字段，避免字段拆分吞掉根 destructor，也避免根 full glue 双 drop 字段。
- `self: deinit T` 参数不再注册普通 lexical cleanup，避免 destructor body 退出时递归/重复 drop 自己。
- 结构化 runtime drop 会继续沿 struct、tuple、array 和 active enum payload 展开已知 custom destructor 子对象；
  generic/associated/opaque cleanup 仍只保留 marker，不凭空生成未知 ABI 调用。

当前仍保守的边界：`Drop` bound、generic Drop impl、trait-object Drop dispatch、async/unwind-aware drop、
panic cleanup ABI 和标准库拥有型资源封装仍是后续设计项；通用 aggregate rollback codegen 已由 M7d-E 补上
compiler-only 子集。LLVM backend 中
`drop` / `drop_if` marker 本身仍是 no-op；M7d-D 的 runtime 行为来自 lowering 阶段额外生成的普通 call。

## M7d-C RAII User Surface 与 Release Closure

M7d-C 的窄 RAII user surface 已完成实现收口。当前实现建立在 M7d-B place-state、M7d-A dropck facts、
M7c lifetime facts、M7b borrow contract 和 M6 resource/drop-glue 基线之上，开放 compiler-owned
`impl Drop for T { fn drop(self: deinit T) -> void { ... } }` 语义表面。

当前新增实现包括：

- parser/AST 接受参数冒号后的 contextual `deinit` 修饰符，AST dump 会输出
  `param self : deinit T`。
- `Drop` 是 reserved destructor surface，不再作为普通用户 trait 注册；`trait Drop`、`impl pkg.Drop for T`、
  `impl Drop[i32] for T`、generic Drop impl、associated type、额外方法、borrow contract、unsafe/extern/export/
  variadic Drop method 等非法 surface 都有专门诊断。
- 合法 destructor 必须精确写成 `impl Drop for T`，目标为 named `struct` / `enum` / `opaque struct`，
  方法必须且只能为 `fn drop(self: deinit T) -> void { ... }`，并带函数体。
- sema 在 `CheckedModule::destructors` 记录 `DestructorInfo`，并把对应 `FunctionSignature` 标记为
  `is_destructor`；checked dump、clone/copy 和 stable fingerprint 都覆盖该事实。
- resource classifier 将带 custom destructor 的类型归为 conservative owned resource；
  drop-glue plan 先生成 `custom_destructor` step，再继续展开结构字段、tuple、array、enum payload、generic
  或 opaque cleanup。
- dropck action 使用 destructor info fingerprint 作为 custom destructor key，并在 drop facts 中记录
  destructor function；`TypeCheckBodyAuthority` 混入 destructor count/fingerprint；IDE hover 显示
  `destructor=custom`。
- IR verifier 已补充 `drop` / `drop_if` target mutable 约束，拒绝对 `*const T` / `&T` target 发出 drop。

M7d-C 本身只声明 semantic/checking/tooling closure；M7d-D 已在其后补上可静态解析 custom destructor 的
runtime direct-call lowering。用户可写 `Drop` bound、generic Drop impl、trait-object Drop dispatch、
async/unwind-aware drop、panic cleanup ABI 和标准库拥有型资源封装继续作为后续设计项。

## M7d-B Struct Field Place-State 与字段级 Drop Flag 子集

M7d-B 的 struct field 子集已完成实现收口。当前实现建立在 M7d-A dropck facts、M7c lifetime facts、M7b
borrow contract 和 M6 cleanup/drop lowering 之上，把本地 owned struct 字段纳入 place-level resource state。

当前新增实现包括：

- body-flow assignment 对直接 local 和 field projection 生成 `reinit`；block cleanup 对 droppable struct
  fields 生成 projected `cleanup_storage`。
- generic template body-flow 会读取 generic side table 的 expr/local 类型，保证 `Box[T]` 这类模板内字段 cleanup
  能看到字段类型。
- sema 允许本地 owned struct 字段 move-out 和字段 reinit；`current.left = replacement` 这类本地字段覆盖进入
  place-state，而通过 `&mut Box[T]` 的 `box.value = replacement` 仍保守拒绝。
- place-state facts 记录 struct field partial move、reinit、cleanup 和 drop flag 状态；owned generic return
  summary 不再被 place-state 误解释为对 moved 参数的借用访问。
- IR lowering 为 droppable struct fields 建立字段级 cleanup/drop flag，在字段 move-out 后关闭对应 flag，在字段
  reinit 后重新打开，cleanup 只 drop 仍 initialized 的字段。
- 样例中 `move_partial_field` 从负例移为正例，并新增字段 reinit 正例。

当前仍未完成的 M7d-B 设计项后来已有部分收口：tuple `.0` / `.1` source surface、本地 tuple 元素 partial
move/reinit/drop flag 已由 M7d-F 补上。indexed move-out、consuming pattern payload、non-`Copy` `?` payload
transfer 仍保守拒绝；`replace` / `take` /
`swap` 还没有 compiler-known primitive 或标准库 intrinsic；borrowed/reference 字段 resource overwrite 仍因缺少本地
drop flag proof 而拒绝。

## M7 Hardening Performance Closure

M7c/M7d 进入下一阶段前的硬化闭环已完成，记录在
[M7 Hardening Performance Closure](m7-hardening-performance-closure.md)。

本轮收口内容包括：

- 剩余 `u32/i32/usize` 审计：query authority 与 checked semantic count 保持/扩展到 `u64`；
  `NormalizedAstOverlay` 计数从 `usize` 改为 `u64`；AST/IR/sema handle、stable key schema、lifetime/body-flow
  index 和 bounded 小域 index 保持 `u32`，作为未来独立 schema migration 处理。
- statement control-flow query 结果按 `StmtId` 四路缓存，并缓存子语句/子块结果；body-loan local-check precheck
  从两次表达式子树扫描合并为一次扫描。
- 新增 `tools/m7_hardening_perf.py`，统一 Google Benchmark、hyperfine 和 `/usr/bin/time -v` 输出。
- 同机同命令 Release benchmark 对照基线提交 `eef0c25b`，broad frontend case 均在当前容器噪声范围内，无可见回退；
  `SemaAstBulk/4096` 当前 CPU time 为约 `20.352 ms`，4x statement 规模约 `4.35x`。
- `perf` 已安装但当前容器 `perf stat` 不可用，因此本轮不把 PMU counter 作为 gate；hyperfine 和
  `/usr/bin/time -v` 已写入 `build/m7_hardening_perf/summary.md`。

## M7c-C Storage Escape 与性能收口

M7c-C 的核心 storage escape 迁移和性能收口已完成，文档入口仍为
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

当前新增实现包括：

- `FunctionBorrowSummary::storage_escapes`，用于记录 non-name storage assignment 中逃逸到外部 storage 的
  borrowed origin。
- lifetime collector 将 borrow summary storage escape 映射为 `local_escape` / `unknown_escape` violation，
  由 lifetime enforcer 统一发出主诊断。
- `TypeCheckBodyAuthority`、query fingerprint、checked dump、borrow summary dump、IDE semantic fact 和 hover
  暴露 storage escape / local escape / unknown escape 状态。
- 旧 `BorrowEscapeAnalyzer` 只在 summary 缺失或 summary 无 storage escape 且 body 有候选 non-name assignment
  时作为窄 fallback guard 运行，避免 summary/lifetime 主路径与旧 analyzer 重复报错。
- borrow summary、lifetime 和 body loan hot path 的 O(n²) 扫描已改为 hash index、sparse graph/cache 或
  per-action/per-carrier queue：origin 去重、lifetime duplicate facts、lifetime violation、outlives reachability、
  body-loan carrier binding、reborrow parent 绑定和 two-phase activation matching 均已收口。

本地实际性能数据使用 `aurexc --profile-output`、`cmake -E time` wall-time spot-check 和 `gprof` 采集。
`build/full-llvm-fedora` 当前配置下，storage escape 压测 `sema.analyze` 3-run median 为：500 条
50.701 ms，1000 条 100.202 ms，2000 条 204.232 ms，4000 条 419.745 ms。优化前同类 500/1000/2000
条为
323.151 / 1007.543 / 4821.837 ms。更新后的 `gprof` 2000 条报告确认旧热点
`BodyLoanSolver::expr_result_contains_loan` 已不再出现在热点调用列表，`BodyLoanSolver::type_contains_reference`
调用数为 4000 次，随输入规模线性增长。

## M7c-A / M7c-B Lifetime Facts 与 Region Enforcement 实现收口

M7c-A / M7c-B 已完成实现收口，文档入口仍为
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

当前实现包括：

- `CheckedModule::lifetime_facts`、`type_lifetime_infos`、`generic_lifetime_predicates`，以及对应 dump、clone、
  interner rebind 和 stable fingerprint。
- `FunctionLifetimeFacts` 中的 region、outlives constraint、type-outlives constraint、return region、violation 和
  body-flow point live-range facts。
- lifetime collector / solver / enforcer 分层：signature/reference origin facts、`@borrow(...)` 与 inferred
  `BorrowSummary` 统一映射到 lifetime facts；solver 做确定性 outlives fixed point 与 live-range projection；
  enforcer 处理 elision ambiguity、return-origin subset、type-outlives 和 local/temporary escape 主诊断。
- `TypeCheckBodyAuthority` 和 IDE `lifetime_facts` detail 混入 lifetime live-range、type/generic predicate count、
  local/unknown escape 状态和 lifetime fingerprint。
- `BorrowSummaryBuilder` 追踪 `strraw` raw pointer alias 的本地来源；raw-derived local return 由新 lifetime checker
  诊断，unsafe raw pointer parameter helper 仍保守记录 unknown return fact。
- 旧 `BorrowEscapeAnalyzer` 已从 return escape 主路径降级为 storage-only parity guard，继续覆盖尚未迁移的
  assignment into escaping field 等存储逃逸场景。

M7c-A/B 仍不声明 M7c-C 范围：public/prototype/extern/trait lifetime release policy、trait impl contract subset
细化、旧 analyzer 删除、closure capture placeholder 落地和更完整的 borrowed-view escape parity matrix 留给下一阶段。

## M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线

M7c/M7d 设计基线已固定，文档入口为
[Aurex M7c/M7d Complete Borrow、Lifetime 与 RAII Drop Check 设计基线](m7c-m7d-complete-borrow-raii-design.md)。

该基线建立在 M6 resource/drop facts、M7a `BodyFlowGraph` / `BodyLoanCheckResult` / `BorrowSummary` 和 M7b
`FunctionBorrowContract` / reborrow / two-phase receiver 之上。后续 M7c 目标是完整 safe borrow 与 lifetime core：
contextual `origin` 参数、`&[origin] T` / `&mut[origin] T`、origin union、region solver、type-outlives、
trait/generic lifetime predicates 和 `BorrowEscapeAnalyzer` 退场。M7d 目标是 RAII/dropck 地基：dropck facts、
destructor body safety、generic drop glue type-outlives、place-level resource state、partial move/reinit/drop flag
以及 RAII user surface。

语法决策：不采用 Rust apostrophe lifetime，也不新增 `ref[...] T` 作为 source surface。显式 origin 绑定到现有引用前缀：
`&[data] T` 表示 shared borrowed view，`&mut[data] T` 表示 mutable borrowed view，`&[left | right] T`
表示 origin union。函数边界继续优先使用 `@borrow(return = [...])`。
该语法只在 type context 中解析，不占用未来 lambda/closure 的表达式语法空间。完整 closure/lambda capture 后续独立设计；
M7c/M7d 只预留 `ClosureCaptureFact` / `ClosureEnvironmentFact` 与 dropck/place-state 复用路线。

工程决策：`src/sema/internal/` 以后只作为 private implementation root，不再直接新增文件；M7c/M7d 新代码必须按
`borrow/`、`lifetime/`、`dropck/`、`place/`、`diagnostics/`、`pipeline/` 等职责拆子目录。其他 compiler stage
同样遵守该解耦规则。全局 `compiler-engineering` 与 `cpp-project-standards` skill 已同步该约束。

## M7b Borrow Contract、Reborrow 与 Lifetime Surface 实现收口

M7b WP1-WP7 已完成实现收口，文档入口为
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 设计基线](m7b-borrow-contract-design.md) 和
[Aurex M7b Borrow Contract、Reborrow 与 Lifetime Surface 路线图](m7b-roadmap.md)。

M7b 已把 M7a 的 `BorrowSummary` / `BodyLoanCheckResult` 内部 facts 提升为函数边界
`FunctionBorrowContract`，引入函数声明前装饰器式 `@borrow(return = [param, self])`，补齐 trait/generic
borrowed-return contract、summary-vs-contract enforcement、reborrow parent/child loan、method receiver access、
receiver auto-borrow two-phase reservation/activation，以及 query/cache/tooling 投影。

当前实现包括：

- `CheckedModule::borrow_contracts`、`FunctionBorrowContract` fingerprint/dump/query 投影。
- `BodyLoan::parent_loan` reborrow model，child effective place 归一到 parent 底层 place，并保留 known-disjoint
  field projection 放宽。
- `FunctionCallBinding` / `TraitMethodCallBinding` 的 `receiver_access`、`receiver_auto_borrow` 和
  `receiver_two_phase_eligible`。
- `BodyTwoPhaseBorrow` reserve/activate facts，reservation conflict 与 activation conflict diagnostics。
- `TypeCheckBodyAuthority` 的 borrow contract/body loan fingerprint、reborrow/two-phase counts 和
  diagnostics-emitted 状态位。
- IDE semantic fact detail 中的 `borrow_contract` 与 `body_loan_check loans/reborrows/two_phase/conflicts`。

M7b 明确不做 full Rust-style lifetime generics、full Polonius Datalog、raw pointer alias safe proof、partial
move / replace / take / swap 完整 place-level resource semantics、`dyn Trait`、async drop、generator borrow
或用户级析构器语法。M7d-C 后续已补上窄 `impl Drop` / `deinit` semantic surface；M7d-D 后续已补上静态
custom destructor call lowering。
M7d-B 后续已补上本地 owned struct field partial move/reinit/drop flag 子集，M7d-F 后续已补上本地 tuple
element partial move/reinit/drop flag 子集；indexed move-out 和 replace/take/swap 仍不属于当前已完成范围。

## M7a CFG-sensitive borrow facts release closure

当前实现阶段是 M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking。M7 设计基线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 设计研究](m7-origin-loan-lifetime-design.md)，
执行路线记录在
[Aurex M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking 路线图](m7-roadmap.md)。

M7-WP2 Phase 1 已完成 collect-only BodyFlowGraph facts：`CheckedModule::body_flow_graphs` 按
`FunctionLookupKey` 保存函数体 point、edge、place 和 action timeline；内部收集器覆盖 statement/expression
entry-exit、顺序点、branch、return、call、defer cleanup、read/write/move-candidate 以及 shared/mutable
borrow action，并提供稳定 dump。该阶段只生产事实，不新增 diagnostics，不替换 `BorrowEscapeAnalyzer`，不改变
M6 move/resource/cleanup 行为。

M7-WP3 Phase 2/3 已完成本地 local loan checker：`CheckedModule::body_loan_checks` 现在保存
`Origin` / `Loan` / conflict facts、shadow/enforced diagnostic mode、projection-aware conflict 结果和稳定
dump。checker 复用 Phase 1 `BodyFlowGraph`，用 carrier-local liveness 支持直接本地 borrow 的 last-use 后写入，
对 active shared/mutable loan 与 write、owned-consume move、read、shared/mutable borrow 的冲突进行 shadow 记录；
语义管线已启用 enforced diagnostics，并给出冲突 primary diagnostic 和 loan 创建 note。`move_candidate` 只有在
M6 `OwnedUseMode::owned_consume` 时才作为 move 冲突，普通 read/copy 不误报。

M7-WP4 已完成 `BorrowSummary` 与 borrowed-return facts：`CheckedModule::function_calls` 记录普通函数、泛型函数、
普通方法和泛型方法的 direct call binding；`CheckedModule::borrow_summaries` 按 `FunctionLookupKey` 保存函数级
`FunctionBorrowSummary`，包含 parameter/local/temporary origins、return origin dependency set、unknown-return
标志、local/temporary escape 标志和 stable fingerprint。call wrapper 会把 callee summary 的 parameter dependency
映射到 caller 实参；generic parameter 与 associated projection 在 summary 内按可能含借用处理，保证 `T = &U`
实例不会漏掉 parameter-origin dependency；函数值调用、callee 缺 summary、raw/unchecked pointer path 和 callee
local/temporary return 不会被当作 safe proof，而是 conservative unknown。

M7-WP5 已补齐 projection/drop/reinit/cleanup matrix：整 local assignment 生成 `reinit`，field/index/deref
assignment 保持 `write`，词法 block local cleanup 生成 `cleanup_storage` invalidation；loan checker 对
write/reinit/drop/cleanup/move/read/shared-borrow/mutable-borrow 使用同一套 projection-aware conflict policy。当前语言
仍没有显式 user drop syntax，所以 `drop` 已作为 checker action 支持，source-level drop emission 仍等待后续语法或
lowering 接入。

M7-WP6 已完成 diagnostics、query 与 tooling projection：`TypeCheckBodyAuthority` 混入 borrow summary / body
loan check fingerprint、origin/dependency/loan/conflict count 和 unknown/local-escape/diagnostic-emitted 状态位；
CLI incremental-cache subject collection 与 IDE snapshot query collection 都消费同一份 `CheckedModule`
facts。IDE semantic facts 新增 `borrow_summary` 与 `body_loan_check`，函数 hover 可展示 summary dependency；
enforced diagnostics 现在包含 primary conflict、loan creation、invalidating action 和可定位时的 later carrier use
note，同时保持按冲突点/range 的 cascade suppression。`dump_checked_module` 输出 `body_loan_checks` summary 与
stable fingerprint。

W7a release closure 已完成性能/内存边界收口：普通 `--check` 路径保留 `BodyLoanCheckResult` 与
`FunctionBorrowSummary` 等稳定 checked facts，不长期保留 full `BodyFlowGraph`；checked/typed 输出和 IDE/tooling
仍可保留 CFG facts。非借用返回函数的 summary 构建走 fast path，不扫描完整函数体；direct/trait call binding
由 `CheckedModule` 维护 expr-id index。release 全量测试、coverage、query sanitizer、perf/stress gate 已验证通过。

M7-WP7 已完成 release closure 文档边界：当前仍不移除 `BorrowEscapeAnalyzer`。WP4/WP6 summary 已记录
borrowed-return facts，但旧 borrowed-local escape 诊断继续由现有 analyzer 负责；只有在 parity 覆盖当前
borrowed-view 逃逸矩阵后才讨论降级/移除。M7a 继续明确不做完整 Rust-style lifetime surface、full Polonius
Datalog engine、raw pointer alias safe proof、用户级析构器语法、partial move / replace / take / swap
完整 place-level resource semantics、`dyn Trait`、async drop 或 generator borrow。M7d-C 后续已补上窄
`impl Drop` / `deinit` semantic surface；M7d-D 后续已补上静态 custom destructor call lowering。

## M6-WP2/WP3/WP4/WP5/WP6/WP7 资源、cleanup、drop-glue 与 tooling 基线

M6 Resource And Access Semantics 已作为 M7 输入基线收口。M6-WP1 已完成三轮设计审视，完整设计基线记录在
[Aurex M6 资源、值生命周期与访问语义调研和三轮设计审视基线](m6-resource-access-semantics-design.md)，
执行路线记录在 [M6 资源、值生命周期与访问语义路线图](m6-roadmap.md)。

M6-WP2/WP3/WP4/WP5/WP6/WP7 已完成 M6 实现基线：compiler-owned `Copy`、内部 `Discard` / `NeedsDrop` /
ownership resource summary、结构化类型分类、stable resource fingerprint、checked dump resource summary、
expression owned-use side table、whole-local move analysis、move 后重新初始化、consume-origin diagnostics、
lexical cleanup-action stack lowering、`defer` 组合、drop flag，以及正式 IR `drop` / `drop_if` cleanup 节点。
收口内容还包括 `BodySlotKind::destructor_drop` destructor body identity、stable drop-glue key、
target-independent drop-glue planner、IDE resource hover projection、generic parameter hover fallback 和
`aurex-lsp` stdio 入口。M7d-B 后续已开放本地 owned struct field partial move/reinit，并在 IR lowering 中使用
字段级 drop flag；M7d-C 后续已开放窄 `impl Drop` / `deinit` semantic surface；M7d-F 后续已开放 tuple
数字字段访问和本地 tuple 元素 partial move/reinit/drop flag。indexed move-out、consuming pattern payload 和
non-`Copy` `?` payload transfer 仍然拒绝；backend custom destructor call lowering 已由 M7d-D 补上。

下一实现包是 M7 CFG-Sensitive Origin、Loan 与 Lifetime Checking。完整 borrow checker、lifetime surface、
partial move、`dyn Trait`、region、async drop、全量 array ABI 解禁和标准库重建继续后移。

## M5 default trait methods release baseline

当前文档基线是 M5 default trait methods release。M5 建立在已经收口的 M4 trait/protocol release baseline
之上，已经收口一个聚焦的 M4 后设计流：nominal static trait 上的 default method body。

M5 设计基线记录在
[Aurex M5 Default Trait Methods 调研与设计基线](m5-default-trait-methods-design.md)，阶段路线见
[M5 Default Trait Methods 路线图](m5-roadmap.md)，发布契约记录在
[Aurex M5 Default Trait Methods Release Baseline](m5-release-baseline.md)。M5 范围是 trait-owned default
bodies、显式 method origin、impl override vs inherited-default completeness、单态化后的 static direct-call
lowering，以及 tooling / diagnostics / query projection。

M5 不包含 dynamic trait object、object safety、vtable ABI、specialization、associated constants、default
associated types、generic associated types、blanket impl、package-level coherence expansion、class-like sugar 或
resource semantics。

## M4 trait/protocol release baseline

当前已实现的 trait 基线是 M4。M4 建立在已经收口的 M2 language-core-no-std、M2.5 frontend/query foundation
和 M3 module/generic/query-backed compiler architecture 之上，完成 nominal static trait、显式 trait impl、
generic trait predicate、static trait method dispatch、associated type，以及 IDE/tooling/diagnostics 投影。

发布契约记录在 [Aurex M4 Trait / Protocol Release Baseline](m4-release-baseline.md)。M5 从该基线出发，不重新打开
M4 WP1-WP8。

## M2 language-core-no-std

M2 从 M1 的失败经验中收缩出来：停止继续修补 M1 的标准库、自举和系统样例路线，转而冻结标准库、删除干扰项，并重新聚焦语言核心设计。

M1 已经舍弃。它的问题是扩张面过大：标准库、host support、构建工具样例、自举实验和语言语义同时推进，导致核心语法与类型规则没有稳定下来。M2 不再把 M1 的 std/selfhost/build-tool 产物当作当前基线。

已完成：

- 删除 `std/` 源树和 host-c support。
- 删除 driver 标准库查找、import path 注入、support source 链接和相关头/源文件。
- 删除 CLI 的 `--stdlib`、`--std-backend`、`--no-stdlib`。
- 删除安装规则中的 `share/aurex/std`。
- 删除 std/M1/system 样例和 std 专项测试。
- 将 `Result` / `Option` 相关语言样例改为自包含定义。
- 将 defer/variadic 样例改为局部 `extern c`，不依赖 std 包装。
- 移除语义层 `std.core.vec/map/result` 专用 ownership 约束。
- 移除 M1 的语言级 `move(...)`、`noncopy struct` 和 use-after-move 追踪。
- 更新测试清单，使 sample suite 回到语言核心。

当前有效基线：

- C++20 Stage0 编译器。
- 手写 lexer/parser 和 ID-backed AST。
- sema、Aurex IR、IR verifier、pass pipeline。
- LLVM backend 和 clang native 输出。
- 自包含 positive/negative `.ax` 语言样例。

风险控制：

- 保留 match payload、`?`、`for`、`defer` 和基础类型系统的语言级正/负例。
- 保留 native hello、IR/LLVM lowering、安装后 compiler 执行验证。
- 后续资源约束不再通过标准库 hardcode 添加，应作为独立资源语义专题设计。

明确暂缓：

- 重新设计库层；不是复活旧 `std`。
- 重新评估 selfhost/Stage1。
- 重新设计 M1 build tool / system examples。
- 重新评估 host support 自动链接。

这些内容必须等 M2 基础语法、值语义、`unsafe`、slice/string 和泛型约束稳定后再重新评估；拥有型资源库还需要资源语义专题完成。
