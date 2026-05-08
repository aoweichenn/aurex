# 运行流程文档

## 编译流水线

1. `aurexc` 解析命令行参数并构造 `CompilerInvocation`。
2. `ModuleLoader` 加载根文件，按导入者目录和显式 `-I` 解析 import。
3. lexer 生成 token，parser 生成 AST。
4. semantic analyzer 合并模块后的 AST，完成名称解析、类型检查、泛型实例化、所有权检查和控制流检查。
5. `--emit=checked` 之前的模式在对应阶段直接输出。
6. IR lowering 生成 Aurex IR，pass pipeline 可按 `--opt-level` 运行。
7. LLVM backend 生成 LLVM IR。
8. native 模式把临时 LLVM IR 文件交给 clang，输出 asm/object/executable。

## 模块查找

本分支只有两个查找来源：

1. import 发起文件所在目录。
2. CLI 显式传入的 `-I path`。

没有标准库根、没有环境变量查找、没有安装前缀推导，也不会在 executable 模式下追加 host support 源文件。

## native 输出

native 输出只编译当前 Aurex 程序生成的 LLVM IR。需要 libc 函数时，源文件自己声明窄 `extern c` 边界，clang 按平台默认链接规则解析 libc。
