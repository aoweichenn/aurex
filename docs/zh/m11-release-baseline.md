# Aurex M11 Principal-Set Composition Release Baseline

日期：2026-06-08

状态：M11a、M11b、M11c、M11d 和 M11e 已完成。M11 的 release baseline 是
origin-bound borrowed dyn principal-set composition，不是标准库阶段，也不是 owning dyn 阶段。

## 1. Release 结论

M11 在 M8 borrowed dyn runtime dispatch、M9 dyn ABI/tooling facts 和 M10 borrowed dyn supertrait upcast
的基础上，完成了 Aurex 自己的 **borrowed principal-set dyn composition**：

- M11a 选择 principal-set borrowed dyn composition 作为 advanced dyn 主线，并固定
  `principal_set_metadata_v1`、required facts 和 non-goals。
- M11b 新增 `PrincipalSetCompositionFacts` query prototype，覆盖 principal-set identity、witness set、
  principal-qualified method namespace、associated equality merge 和 projection facts。
- M11c 接入用户可写 `dyn (A + B)` borrowed composition spelling、parser/AST/type/sema、coercion check、
  associated equality merge diagnostics 和 direct composition method-call guard。
- M11d 接入 `trait_object_composition_pack` / `trait_object_composition_project` IR、`PrincipalSetMetadataLayout`、
  verifier invariants、LLVM `{ [N x ptr] }` metadata global 和显式 composition-to-principal runtime projection。
- M11e 把 M11d runtime surface 收口到 query/cache/tooling：`FunctionDynAbiFacts` 现在暴露 principal-set
  metadata 和 composition projection runtime descriptors，lower-IR query fingerprint 与 IDE semantic fact/hover
  不再需要反推 IR dump。

一句话：M11 把 borrowed multi-principal dyn view 做成了可解析、可检查、可 lowering、可验证、可执行、可查询、
可缓存、可 IDE 投影的 release baseline，但仍只覆盖显式 projection 子集。

## 2. 当前能做什么

当前可以写：

```aurex
trait Draw {
    fn draw(self: &Self) -> i32;
}

trait Debug {
    fn debug(self: &Self) -> i32;
}

struct File {
    value: i32;
}

impl Draw for File {
    fn draw(self: &File) -> i32 {
        return self.value;
    }
}

impl Debug for File {
    fn debug(self: &File) -> i32 {
        return self.value + 1;
    }
}

fn score(view: &dyn (Draw + Debug)) -> i32 {
    let draw: &dyn Draw = view;
    let debug: &dyn Debug = view;
    return draw.draw() + debug.debug();
}

fn main() -> i32 {
    let file: File = File { value: 7 };
    let view: &dyn (Debug + Draw) = &file;
    return score(view);
}
```

`&file -> &dyn (Debug + Draw)` 是 concrete-to-composition borrowed coercion。`let draw: &dyn Draw = view`
是 explicit composition-to-principal projection。Projection 从 `principal_set_metadata_v1` metadata 中按 canonical
principal index 加载目标 single-trait vtable，再构造普通 `{data*, vtable*}` borrowed dyn view。

当前仍不能写：

```aurex
fn score(view: &dyn (Draw + Debug)) -> i32 {
    return view.draw();
}
```

`view.draw()` 这类 direct composition receiver dispatch 仍被拒绝。M11 没有引入 principal-qualified method syntax，
也没有把多个 principal 的 slots flatten 到一个未命名 namespace。

## 3. Runtime 与 ABI 边界

M11 的 runtime representation 是 borrowed-only：

- Single-trait dyn view 仍是 M8/M10 的 `{data*, vtable*}`。
- Principal-set composition view 是 `{data*, principal_set_metadata*}`。
- `principal_set_metadata_v1` global shape 是 `{ [N x ptr] }`。
- Metadata entries 按 principal-set type 的 canonical principal order 排列。
- 每个 entry 指向一个 single-trait vtable witness。
- 同一个 principal set 对不同 concrete type 生成不同 metadata global，key 是
  `(principal_set_identity, concrete_type)`。

Projection 不改变：

- data pointer。
- source origin。
- borrow permission。
- cleanup obligation。
- ownership。

Metadata 不包含：

- owner container。
- allocator slot。
- destructor slot。
- dynamic Drop dispatch。
- size / align / type metadata。
- value buffer。

## 4. Query、Cache 与 Tooling 收口

M11e 新增并固定了 runtime-facing dyn ABI facts：

- `DynMetadataPolicy::principal_set_metadata_v1`。
- `DynPrincipalSetWitnessAbiDescriptor`。
- `DynPrincipalSetMetadataAbiDescriptor`。
- `DynCompositionProjectionAbiDescriptor`。
- `FunctionDynAbiFacts::principal_sets`。
- `FunctionDynAbiFacts::composition_projections`。
- `DynAbiFactsSummary::principal_set_metadata_count`。
- `DynAbiFactsSummary::principal_set_witness_count`。
- `DynAbiFactsSummary::composition_projection_count`。

这些 facts 参与 validation、summary、dump 和 stable fingerprint。`lower_function_ir_result_fingerprint()` 现在混入
principal-set metadata/projection counts，所以 composition runtime descriptor 改变会正确 invalidate lower-IR query
result。IDE semantic facts 和 function hover 会展示：

- `metadata=principal_set_metadata_v1`。
- `principal_sets=N`。
- `composition_projections=N`。
- 首条 composition projection source/target。
- principal index。
- borrow kind。
- projection metadata policy。

M11e 还固定了一个重要边界：如果函数只接收 `&dyn (A+B)` 并投影到 `&dyn A`，但没有在该函数内从 concrete
source pack composition view，则该函数只能记录 projection descriptor；它不能伪造 concrete-specific principal-set
metadata descriptor。Concrete-specific metadata descriptor 只在能静态看到 composition pack 的函数中记录。

## 5. Verifier 与测试收口

M11e 对 verifier hardening 做了负例补强：

- duplicate witness index 会被拒绝。
- metadata identity drift 会被拒绝。
- composition pack 找不到 matching metadata layout 会被拒绝。
- composition project principal index 越界会被拒绝。
- composition project 缺失或不匹配 principal object 会被拒绝。

Focused tests 覆盖：

- Query descriptor validation、fingerprint、dump、lower-IR invalidation。
- IR dyn ABI facts 对 concrete metadata 和 callee projection facts 的边界区分。
- IDE semantic fact / hover 对 composition runtime facts 的展示。
- IR verifier metadata/projection drift negative matrix。
- 既有 LLVM principal vtable load 和 native execution dispatch。

覆盖率门槛仍是 90%。M11e 不通过降低质量来凑覆盖率；新增测试锁住 facts、fingerprint、verifier invariant 和用户可见 tooling surface。

## 6. 明确非目标

M11 release baseline 继续不实现：

- 标准库。
- `Box<dyn Trait>`。
- owning dyn / owning existential container。
- allocator API / allocator policy。
- dynamic Drop dispatch。
- trait-object destructor ABI。
- owner/value-buffer metadata。
- bare `dyn A + B` parser syntax。
- direct principal-qualified composition method dispatch。
- auto trait composition。
- structural conformance。
- consuming receiver dyn dispatch。

这些不是永久禁止，但必须进入独立后续阶段重新设计、估算、实现和测试。M11 只收口 borrowed composition view。

## 7. 代码量偏差

M11e 的早期估算是 700-1,300 行新增/修改。代码和测试实现本身落在该范围内；最终 release diff 会因为新增
M11 release baseline 文档、状态入口和 documentation tests 而高于纯代码实现量。

偏差来源主要是：

- `FunctionDynAbiFacts` 和 lower-IR query/IDE tooling 已由 M9/M10 建好，本次只扩展 composition-specific descriptors。
- M11d 已完成 IR/backend runtime，本次不新增 runtime representation 或 LLVM lowering shape。
- Verifier 负例复用真实 lowered module mutation，不需要新建大型手造 IR fixture。
- 文档收口需要同步 README、progress、version、next-steps、usage、language manual、feature inventory、architecture、
  requirements 和 documentation tests，这部分是 release closure 成本。

这个偏差属于 release 文档和测试收口带来的正向范围膨胀，不表示 M11e 偷偷实现了标准库、owning dyn 或 direct
composition dispatch。

## 8. 下一步

M11 已结束。下一步建议进入 **M12 Advanced Dyn Design Baseline**，先做设计研究和 policy selection，而不是直接
实现标准库或 owning dyn。

M12 应从 M11 仍未覆盖的 advanced dyn 能力里选择一个独立主线，例如：

- direct principal-qualified composition method dispatch。
- owning dyn / `Box<dyn Trait>`。
- dynamic Drop dispatch。
- allocator policy。
- auto trait / marker trait composition。

其中 owning dyn、`Box<dyn Trait>` 和 allocator policy 仍依赖标准库或 runtime ownership policy；dynamic Drop dispatch
需要 destructor ABI、dropck/tooling facts 和 runtime cleanup model；direct composition method dispatch 需要先设计
principal-qualified syntax 和 ambiguity diagnostics。M12 不能把这些能力偷偷塞进 M11 的
`principal_set_metadata_v1` borrowed metadata。
