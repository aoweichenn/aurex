# 设计实现文档

## 模块加载

`ModuleLoader` 把根文件和 import 文件合并为一个 `AstModule`。加载器记录正在加载和已经加载的文件，防止循环导入和重复合并。导入文件必须声明与 import 路径一致的 `module`。

查找路径只有导入者目录和显式 `-I`。标准库自动注入已删除。

## 语义分析

语义层当前负责：

- 名称、模块、可见性和 re-export。
- 类型解析、const 检查、layout 检查。
- generic function/struct/enum/method 实例化。
- pattern matching 与 exhaustiveness。
- 普通值语义检查，以及数组/含数组类型不能作为当前 by-value 存储或赋值目标的限制。
- `for`、`defer`、`break`、`continue` 的控制流和 lowering 前约束。

M1 的 `noncopy` / `move` / use-after-move 语义已从 M2 当前实现删除。std 专用 ownership hardcode 也已移除。当前实现不维护语言级 copy/drop/move 状态；后续资源约束作为独立资源语义专题重新设计。

## 后端

AST 先 lowering 到 Aurex IR，再通过 pass pipeline 和 LLVM backend 生成 LLVM IR。native 输出由 clang 完成。executable 模式不再追加任何标准库 support source。

## 测试实现

测试 harness 会把可缓存的 compiler 调用直接走 C++ driver，避免每个用例都启动脚本/进程。native 输出仍需要实际调用 clang 和执行生成的二进制。
