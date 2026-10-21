# CK7 反向优化详细规范

> 本文件详细说明反向优化的类型、场景、委托执行流程和清单格式。SKILL.md 中只保留核心规则。

## 3 种反向优化类型

| 类型 | 触发 | 动作 |
|------|------|------|
| ① 不一致修正 | 函数名/行号/签名与源码不符 | 修正值 |
| ② 遗漏补充 | L3/L4 发现子模块文档未记录的约束/返回值/机制 | 补充内容 |
| ③ 不准确修正 | 子模块文档描述与源码实际逻辑不符 | 修正描述 |

## 8 个主动发现场景

在 CK1V_SCAN L3/L4 验证过程中，以下发现**必须**记录到反向优化建议清单：

| # | API 验证发现 | 类型 | 优化位置 |
|---|------------|------|---------|
| 1 | L3 追踪子模块函数 return 路径，发现子模块返回值表遗漏错误码 | ② | 子模块 §3 返回值 |
| 2 | L4 构建约束矩阵，发现子模块文档未记录的参数约束 | ② | 子模块 §3 输入说明 |
| 3 | L3 发现子模块函数有自动行为，子模块文档未描述 | ② | 子模块 §5 设计要点 |
| 4 | L3 发现子模块函数调用了其他子模块，依赖关系未记录 | ② | 子模块 §1 依赖关系 |
| 5 | L4 发现参数语义描述不准确 | ③ | 子模块 §3 输入说明 |
| 6 | Grep 源码发现行号偏差 >5 行 | ① | 子模块全文 |
| 7 | L3 发现函数签名与文档标注不符 | ① | 子模块 §3 函数原型 |
| 8 | L4 发现枚举值未记录 | ② | 子模块 §3 接口规格 |

## 委托 driver-design-md-generate skill 的执行规范

> **核心约束**：API skill 不直接修正子模块文档。委托 driver-design-md-generate skill 独立验证后修正。

```text

API skill（CK1V_SCAN）
  ├─ L3/L4 验证深入子模块函数源码
  ├─ 发现子模块文档遗漏/不准确
  └─ 记录到"反向优化建议清单"（仅建议，不修正）
       │
       ▼ 委托
driver-design-md-generate skill（CK7_REVERSE_OPTIMIZE）
  ├─ 收到建议清单
  ├─ 逐项独立验证：重新 Read 子模块源码（禁止信任 API 结论）
  ├─ 三方核对：源码 vs 子模块文档 vs API 建议
  ├─ 确认 → 修正子模块 design.md + 标注来源 + 更新 changelog
  └─ 驳回 → 不修正 + 反馈给 API skill（API 理解有误时）

```

**独立验证要求**（CRITICAL）：

- driver-design-md-generate skill 对每项建议必须重新 Read 子模块源码
- 禁止信任 API skill 的结论
- 原因：API 视角是"入参组合规格"，子模块视角是"内部实现逻辑"。API 认为的"遗漏"可能是子模块有意省略的

**典型驳回场景**：API L4 发现 `module_alloc` 的 flag 有 `VA_ONLY` 约束，认为子模块遗漏。但 driver-design-md-generate 验证后发现 `VA_ONLY` 是 API 层设置的，子模块不应描述此约束 → 驳回。

## 反向优化建议清单格式

```markdown

## 反向优化建议清单（待 CK7 委托 driver-design-md-generate skill 验证）

> 以下建议基于 API skill L3/L4 验证，未经子模块 skill 独立验证。

| # | 子模块文档 | 类型 | 发现 | API 验证依据 | 建议动作 |
|---|---------|------|------|------------|---------|
| 1 | malloc_mng_design.md | 遗漏补充 | module_free 返回 CLIENT_BUSY 未记录 | module_alloc.c:89-93 L3 | 建议补充返回值表 |

```

## changelog 更新规则

```text

| v{N+1} | {date} | CK7_REVERSE_OPTIMIZE from {domain}_domain_design.md（经 driver-design-md-generate 独立验证）:

  ① 补充 module_free 返回值（API 来源: module_alloc.c:89 L3，独立验证: malloc_mng.c:{line} 确认）
  ② 修正 module_alloc flag 描述（API 来源: module_alloc.c:232 L4，独立验证: malloc_mng.c:{line} 确认）
  驳回 {M} 项：{驳回原因} |

```

## 禁止事项

- 禁止 API skill 直接修正子模块文档
- 禁止 driver-design-md-generate skill 信任 API 结论而不独立验证
- 禁止发现遗漏但不反向优化
- 禁止不标注来源
- 禁止扩大范围（只修正 API 验证实际发现且经独立验证确认的）
- 禁止不更新 changelog
- 禁止驳回项不反馈
