# 版本边界

当前版本：0.1.9

当前文档基线：中文文档精简版。

## 版本内容

- 文档树收敛为中文版本。
- 删除英文镜像、旧 review、旧 spec、skill 草稿和阶段流水账。
- 当前语言路线以泛型和语法修正为主。
- 闭包 capture-list、array/slice for-in 和 protocol iterator for-in 已经进入当前语言边界。
- 当前文档入口固定为 `docs/README.md` 和 `docs/zh/README.md`。

## 当前语言边界

- 泛型使用尖括号。
- slice 表面使用 `[]T` / `[]mut T`。
- slice / str 基础观察使用 `.len` / `.ptr`。
- counted range loop 暂时保留 `for i in range(...)`。
- `for item in expr` 当前支持 array/slice 按值迭代和用户 protocol iterator。array/slice 元素类型必须满足 `Copy`；protocol iterator 的 `next()` item 不要求 `Copy`。
- closure capture-list 支持 `[]`、`[x]`、`[&x]`、`[&mut x]`、`[=]`、`[&]`、显式覆盖、init-capture 和 move capture。

## 不再维护

- 英文文档镜像。
- 旧语法冻结 spec。
- 独立 review 报告目录。
- skill 草稿目录。
- 每个阶段一篇的历史记录文档。
- 历史设计和专题草案。
