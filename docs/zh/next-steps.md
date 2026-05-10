# 下一步计划

## 当前分支原则

标准库已冻结并从 M2 当前树删除。下一阶段不要继续扩张 std，也不要用 std 样例证明语言能力。所有新能力先用自包含 `.ax` 样例验证，等语法、类型系统、ownership/borrow/drop 规则稳定后再恢复标准库。

## 优先路线

1. 值语义与资源模型重新设计

   M2 当前先删除 M1 的 `move(...)` / `noncopy struct`，避免把失败的 move-only MVP 继续当作语言地基。下一步要先写清普通值传递、struct/enum payload、match payload、`?` 和数组/含数组类型限制的统一规则，再决定 copy/drop/ownership 如何重新进入类型系统。

2. Drop / destructor 设计

   设计语言级 drop capability，而不是复用 M1 的 destructor 约定。需要确定 drop order、early return / break / continue / defer 的交互、泛型 `T: Drop` 约束，以及拥有资源但没有释放能力的类型如何诊断。

3. Borrow 语义

   设计 shared borrow、mutable borrow、borrowed return、aliasing 规则和生命周期区域。短期可先做局部 borrow checker，再扩展跨函数签名。

4. Capability / trait / where

   用语言机制替代临时 hardcode。第一批能力建议从 `copy T`、`drop T` 开始，后续再扩展 `eq T`、`ord T`、`hash T`。语法上需要评估：

   ```aurex
   fn clone_or<T>(value: T, fallback: T) -> T where T: Copy
   fn destroy_all<T>(items: *mut T, len: usize) -> void where T: Drop
   ```

5. 字符串基础类型

   保留 `str` 作为语言级 borrowed UTF-8 slice 的设计方向，但不要恢复 `String`/`Bytes` 标准库实现。先完成 `str` 的类型、ABI、字面量、slice 边界和内建操作边界。

6. 测试性能继续收口

   保持测试 harness 直接调用 C++ driver，避免每个用例启动脚本。新增用例时区分 check/IR/native 三类，只有必须验证运行时行为的样例才生成并执行二进制。

## 明确暂缓

- std 容器、文件、目录、进程、console。
- M1 frontend / axbuild 样例；M1 阶段已经舍弃，不能作为当前路线继续推进。
- host support C shim。
- 安装后 std 查找。

恢复这些内容的前置条件是：ownership、borrow/drop、capability/trait/where 已有稳定语言级设计和测试矩阵。
