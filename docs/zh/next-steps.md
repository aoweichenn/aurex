# 下一步计划

## 当前分支原则

标准库已冻结并从本分支删除。下一阶段不要继续扩张 std，也不要用 std 样例证明语言能力。所有新能力先用自包含 `.ax` 样例验证，等语法、类型系统、ownership/borrow/drop 规则稳定后再恢复标准库。

## 优先路线

1. 所有权模型收口

   明确 copy / move / noncopy enum payload / match payload / `?` 的统一规则。补齐 partial move、字段 move-out、move 后不可访问、条件控制流合流和 loop 中 moved-state 的诊断。

2. Drop / destructor 设计

   把当前 `destroy(self: *mut T) -> void` 识别推进为语言级 drop capability。需要确定 drop order、early return / break / continue / defer 的交互、泛型 `drop T` 约束，以及没有 destructor 的 noncopy 类型如何诊断。

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
- M1 frontend / axbuild 样例。
- host support C shim。
- 安装后 std 查找。

恢复这些内容的前置条件是：ownership、borrow/drop、capability/trait/where 已有稳定语言级设计和测试矩阵。
