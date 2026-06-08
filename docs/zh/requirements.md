# 需求分析文档

## 当前分支目标

当前实现基线是 M8 borrowed dyn trait runtime dispatch closure。M6-WP2 到 M6-WP7 已落地资源分类、whole-local move
analysis、cleanup obligation lowering、drop-glue identity/planning、tooling projection 和 release closure；
M7 已完成 CFG-sensitive borrow/lifetime/resource hardening；M8 已完成 borrowed dyn trait frontend/sema、
checked vtable facts、IR/backend dynamic dispatch 和 hardening closure。
历史基线说明：当前实现基线是 M6 Resource And Access Semantics 这条旧 release baseline 仍作为资源语义
设计入口保留，但当前分支已经继续推进到 M8。
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
- direct supertrait declaration 和 check-only borrowed dyn upcast facts：`trait Child: Parent`、checked
  `TraitSupertraitEdgeFact`、`TraitObjectUpcastCoercionKey` 和 `DynUpcastAbiDescriptor`；runtime upcast lowering
  后移到 M10c。
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
- owning dyn、`Box<dyn Trait>`、allocator、trait-object Drop dispatch、supertrait upcasting runtime lowering、多 trait object
  composition、associated const、default associated type、generic associated type、specialization、minimal implementation
  annotation、完整 Rust-style lifetime surface、async drop 和标准库重建。

M1 的语言级 `move(...)` 和 `noncopy struct` 不再属于 M2 当前需求。M6 不复活这套 ad hoc surface，而是按
`Copy`、`Discard`、`NeedsDrop`、future `MustConsume` 四维模型重新推进资源语义。
