# 实现说明

## 模块加载

`ModuleLoader` 把根文件和 import 文件合并为一个 `AstModule`。导入文件必须声明与 import 路径一致的 `module`。查找路径只有导入者目录和显式 `-I`，没有标准库自动注入。

## 前端

前端分成 lexer、parser、AST 和 sema：

- lexer 生成 token、保留 source range，并识别当前关键字。
- parser 使用递归下降和 recovery 生成 AST；语法规则不依赖空格或注释。
- AST dump 用于测试和调试，必须稳定反映当前语法。
- sema 负责名称、类型、泛型、trait/capability、borrow/resource、控制流和错误诊断。

诊断在创建时携带结构化 kind、category 和 code；message 只负责展示，不能作为语义判断依据。

## 语义和 lowering

语义阶段会记录 checked 类型、lambda 信息、泛型实例、函数签名、borrow summary、move/resource facts 和语句局部类型。IR lowering 从这些 checked facts 生成 Aurex IR。

当前新增语法必须至少完成：

- parser 接受/拒绝形状。
- AST 保存并 dump 关键节点。
- sema 验证类型和语义边界。
- borrow/move/place/generic 扫描能看到新增表达式或语句字段。
- IR lowering 生成可验证 IR。
- positive/negative sample 覆盖运行或诊断。

## 后端

Aurex IR 通过 pass pipeline 和 LLVM backend 生成 LLVM IR。native 输出由 clang 完成。executable 模式只编译当前 Aurex 程序生成的 LLVM IR，不自动追加 host support 源。

## 测试

测试覆盖 parser、sema、IR、integration sample、negative diagnostics 和 native execution。新增语言语法必须优先补样例和 focused integration 测试，再跑相关 gtest 和完整 suite。
