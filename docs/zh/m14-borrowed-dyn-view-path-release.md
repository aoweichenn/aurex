# Aurex M14 Borrowed Dyn View Path Inference Release Baseline

M14 已完成 borrowed dyn view path inference / dispatch release。它把 M13 已经落地的
`dynproject<SourcePrincipal, TargetSupertrait>(view)` runtime chain 扩展为两类受限隐式路径：

- expected-type borrowed projection：当上下文明确需要 `&dyn Parent`，而 source 是
  `&dyn (Child + Debug)`，并且只有一个 principal 能沿 checked supertrait path 到达 `Parent` 时，可以写
  `let parent: &dyn Parent = view;` 或把 `view` 传给 `fn takes_parent(x: &dyn Parent)`。
- direct supertrait method dispatch：当 `view.parent()` 中 `parent` 只存在于唯一 source principal 的 supertrait
  path 上时，sema 会把它解析为 `&dyn (Child + Debug) -> &dyn Child -> &dyn Parent -> Parent::parent`
  的 borrowed view path。

M14 不新增 parser surface，不新增 runtime metadata policy，也不实现标准库、owning dyn、`Box<dyn Trait>`、
allocator、dynamic Drop dispatch、trait-object destructor ABI 或 bare `dyn A + B`。它继续复用：

- M11/M12 的 `principal_set_metadata_v1` 和 `trait_object_composition_project`。
- M10 的 `supertrait_vptr_metadata_v1` 和 `trait_object_upcast`。
- M8 的 ordinary single-trait dyn `vtable_slot` dispatch。

## 当前可写示例

```aurex
trait Parent { fn parent(self: &Self) -> i32; }
trait Child: Parent { fn child(self: &Self) -> i32; }
trait Debug { fn debug(self: &Self) -> i32; }

fn score(view: &dyn (Child + Debug)) -> i32 {
    let parent: &dyn Parent = view;
    return view.parent() + parent.parent();
}
```

`view.parent()` 的语义不是把 composition flatten 成一个大 vtable，也不是动态搜索所有 parent。它只在
principal set 中找到唯一可证明的 source principal，然后生成与显式
`dynproject<Child, Parent>(view)` 相同的 runtime chain。

## Query / Tooling Facts

M14 新增 `BorrowedDynViewPathFact`、`BorrowedDynViewPathUse` 和 summary counters：

- `borrowed_view_path_count`
- `borrowed_view_path_dispatch_count`
- `borrowed_view_path_expected_projection_count`

`BorrowedDynViewPathFact` 会记录：

- source principal object。
- target supertrait object。
- composition projection path fingerprint。
- supertrait edge fingerprint。
- borrow kind。
- use：`explicit_projection`、`expected_type_projection` 或 `method_dispatch`。
- source composition view、projected principal view 和 target supertrait view 名称。
- method dispatch 时的 method name 和 vtable dispatch step。

`principal_set_composition_facts_fingerprint()`、`summarize_principal_set_composition_facts()` 和
`dump_principal_set_composition_facts()` 都已混入该 fact，避免 query/cache/tooling 漏看隐式 borrowed view path。

## 歧义规则

M14 只接受唯一路径：

- 如果两个 principal 都能到达同一个 supertrait method，例如 `dyn (ChildA + ChildB)` 且
  `ChildA: Parent`、`ChildB: Parent`，`view.parent()` 会诊断
  `dyn trait composition method `parent` is ambiguous across multiple principal traits`。
- 如果 expected type 是 `&dyn Parent`，但 source composition 中存在多个可达 source principal，则 assignment /
  argument / return 仍会被拒绝，用户需要显式选择 source principal。
- direct principal method 仍优先于 inherited supertrait method；M14 不改变 M12 的 principal method namespace 规则。

## Runtime Lowering

M14 lowering 复用 M13c 已验证的两步路径：

1. `trait_object_composition_project` 从 `&dyn (Child + Debug)` 中取出 `&dyn Child`。
2. `trait_object_upcast` 沿 `Child -> Parent` supertrait edge 取出 parent vtable。
3. 如果是 method dispatch，再用 ordinary `vtable_slot` 读取 `Parent::parent` slot 并 indirect call。

Function-level dyn ABI facts 仍通过既有 `composition_projections`、`upcasts` 和
`composition_supertrait_chains` 描述 runtime chain；M14 的新增 fact 属于 frontend/query/tooling 的 path
resolution 视角。

## 覆盖

M14 validation 覆盖：

- query fact enum、validation、summary、dump 和 fingerprint drift。
- explicit `dynproject<...>` path 的 `BorrowedDynViewPathFact{use=explicit_projection}`。
- expected-type projection 的 `BorrowedDynViewPathFact{use=expected_type_projection}`。
- direct supertrait method dispatch 的 `BorrowedDynViewPathFact{use=method_dispatch}`。
- ambiguous composition-supertrait dispatch negative case。
- IR lowering 中 direct/expected 两类路径都生成 composition project + upcast + vtable slot。
- native execution 中多 concrete 的 direct supertrait dispatch 会选择正确 parent vtable。
