---
name: driver-spec-md-generate
description: 通用模块 API 规格文档生成 skill。生成顶层 spec.md（域索引）+ 逐 API {API}_spec.md（10项规格）+ 可选 domain_design.md（域级概述+组合模式）。配合 per-module knowledge.md 使用。触发：生成 api 规格文档/api spec/接口规格。
---

# 模块 API 规格文档生成 skill

## 产出结构

```text

output/{module}/{ver}/
├── spec.md                              ← 顶层索引（域表，指向各域目录）
└── 1_support/api_master/api/
    ├── alloc_domain/                    ← 域目录
    │   ├── halMemAlloc_spec.md          ← 每 API 一个独立 spec 文件（10 项规格）
    │   ├── halMemFree_spec.md
    │   └── ...
    ├── op_domain/
    └── ... (8 domains)

```

**可选产出**：`{domain}_design.md`（域级文档，含组合使用模式 §4 + 关键设计要点 §5）。非必须，按需生成。

## 与 driver-design-md-generate 的差异

| 维度 | driver-design-md-generate（子模块） | 本 skill（API 规格） |
|------|----------------------|---------------------|
| 文档定位 | 子模块内部实现 | API 层接口规格 |
| 核心产出 | 架构图+结构体+算法 | **规格矩阵**（约束+合法组合） |
| 调用路径 | 深入子模块内部 | 到子模块边界 `→ 详见 [子模块_design.md]` |
| 调用者分析 | 必须做 | 不做 |
| 组合模式 | 无 | **可选**（domain_design.md §4） |

## 必需参数

| 参数 | 必需 | 说明 |
|------|------|------|
| project_root | 是 | 项目根目录 |
| output_dir | 否 | 默认 `{project_root}/.reverse_analysis/output/{module}/{ver}/1_support/api_master/api/` |
| domain | 否 | 以 knowledge.md §4 为准 |
| submodule_docs_dir | 否 | 默认 `{project_root}/.reverse_analysis/output/{module}/{ver}/` |

## 严禁伪造（CRITICAL）

> **头文件 @brief/@param/@attention 和 README 可能不准确，源码是唯一验证标准。**

- 禁止从头文件 `@brief` 推断函数功能（必须 Read 函数体）
- 禁止从头文件 `@param` 推断参数约束（必须 Read 校验函数）
- 禁止从头文件 `@return` 推断返回值（必须追踪所有 return 路径）
- 禁止从头文件 `@attention` 复制注意事项（必须基于源码校验逻辑）
- 禁止从 README 推断支持场景（必须从源码 feature switch 确认）
- 禁止 `[待验证]`（只允许 `[未验证]`，如 Device 侧源码不在仓库）
- 禁止用正则提取签名替代 Read 声明行

## 执行协议（CK0-CK7）

| 步骤 | 检查点 | 门控条件 |
|------|--------|---------|
| 0 知识加载 | CK0_KNOWLEDGE | 加载 knowledge.md（见 driver-design-md-generate/references/knowledge-template.md） |
| 1 初始化 | CK1_INIT | 路径+API清单+**子模块文档索引**+Git |
| 1.5 验证 | CK1V_SCAN | **四层验证**+子模块交叉验证+反向优化清单积累 |
| 2 声明 | CK2_DECLARE | 声明块 |
| 3 意图 | CK3_INTENT | 用户确认 |
| 4 生成 | CK4_API_SPEC | **逐 API 生成 {API}_spec.md（10项规格）** |
| 5 格式 | CK5_FORMAT | 命名+元信息+变更记录 |
| 6 检查 | CK6_CHECK | **spec.md 域表完整 + 每 API 10项完整 + 0 伪造** |
| 7 反向优化 | CK7_REVERSE_OPTIMIZE | **委托 driver-design-md-generate skill 独立验证后修正子模块文档** |

### CK1_INIT

1. 解析路径参数
2. Read `header_file`，提取目标域 API 声明
3. Glob `api_source_dir/*.c`，确认实现文件
4. Read README（辅助理解）
5. **Glob `{submodule_docs_dir}/**/*_design.md`**，按功能域匹配相关子模块文档，Read 对外接口章节获取函数名/行号/签名
6. **Glob `{output_dir}/{domain}/`**，确认已有 spec 文件（增量刷新时避免重复生成未变更的 API）
7. Git 信息 + model_id

### CK1V_SCAN 四层验证

| 层级 | 验证 | 方法 | 产出 |
|------|------|------|------|
| L1 | 函数声明+实现存在，行号正确 | Grep+Read | 函数原型块 |
| L2 | 参数类型/返回类型/个数准确 | Read 声明行 | 输入表参数类型 |
| L3 | 所有 return 路径追踪+校验逻辑 | Read 函数体+校验函数 | 返回值表+调用路径 |
| **L4** | **flag 位域+约束矩阵+合法组合** | **Read 校验函数所有分支** | **规格矩阵（核心产出）** |

> L1≠L2≠L3≠L4。L4 是本 skill 与 driver-design-md-generate 的核心区别。

#### L4 规格矩阵构建（5 步）

详见 [api-spec-matrix-guide.md](references/api-spec-matrix-guide.md)。

1. **定位校验函数**：Grep API 名→Read 函数体→识别 parse/check/validate 调用
2. **穷举所有 return 路径**：每个 if/switch 分支记录 条件→返回码→源码行
3. **构建约束矩阵**：`| # | 约束条件 | 返回码 | 源码行 |`
4. **构建合法组合示例**：选取典型组合
5. **构建参数特定子表**：flag→位域布局 / struct→字段规格 / enum→取值规格

#### 子模块文档交叉验证 + 反向优化清单

详见 [ck7-reverse-optimize-spec.md](references/ck7-reverse-optimize-spec.md)。

**被动检查**（一致性）：函数名/行号/签名/文档存在性/返回值语义 vs 子模块 design.md

**主动发现**（L3/L4 过程中）：

- 返回值遗漏 → 记录（类型②遗漏补充）
- 约束遗漏 → 记录（类型②）
- 参数语义不准确 → 记录（类型③不准确修正）
- 自动行为/依赖/枚举遗漏 → 记录（类型②）
- 行号/签名不一致 → 记录（类型①不一致修正）

> **发现的问题记录到"反向优化建议清单"，不立即修正子模块文档。CK7 委托 driver-design-md-generate skill 独立验证后修正。**

### CK4_API_SPEC 逐 API 生成

**生成流程（严格按顺序，逐 API 生成后立即自检）**：

1. 如 spec.md 域表不存在或域信息变更，更新 spec.md
2. **逐 API** 生成 `{API}_spec.md`：

   a. 生成当前 API 的 10 项规格
   b. **立即自检**：对照下方 10 项清单逐项检查
   c. 缺失项**立即补充**，不进入下一个 API
   d. 当前 API 10 项完整 → 生成下一个 API

3. （可选）生成 `{domain}_design.md`：域级概述 + 组合使用模式 + 关键设计要点

**10 项自检清单（每个 API 生成后立即执行，不依赖记忆）**：

- [ ] 函数原型（声明位置+实现位置+代码块）
- [ ] 函数功能（基于源码 switch/if 分支分析，体现核心行为）
- [ ] 输入说明（参数表 + 规格矩阵，复杂参数 API）
- [ ] 地址来源规格（仅有地址参数的 API，非地址参数 API 省略）
- [ ] 输出说明（输出参数表，纯输入填"无"）
- [ ] 返回值说明（穷举所有 return 路径）
- [ ] 前置条件与依赖（须先调哪些接口）
- [ ] 支持场景（基于源码 feature switch）
- [ ] 注意事项（基于源码校验逻辑，禁止从 @attention 复制）
- [ ] 调用路径（到子模块边界 `→ 详见 [子模块_design.md]`）

> **禁止**：当前 API 10 项未完整前生成下一个 API
> **禁止**："先完成再完善"策略——宁可少生成 API，也要保证已生成的 API 完整
> **禁止**：跳过自检直接进入 CK6
> **禁止**：将自检清单保留在最终文档中——自检是过程工具，确认 10 项完整后删除自检块
> **禁止**：未实现的 API 加详细说明——只保留函数原型 + "**未实现**" 标记，不加其他内容

### 未实现 API 规则

源码不在当前仓库的 API（`DV_ONLINE` / `DV_OFF_ONLINE` / `#ifndef UVM_OPEN` / Sunset interface 等）只保留：

```markdown

#### {API名}

**函数原型**:

```c

// 声明: pkg_inc/ascend_hal_base.h:{L}
DLLEXPORT ... {API名}(...);

```text

**未实现**：当前路径无实现（`{原因}`）。

```

**不加**：函数功能、输入说明、输出说明、返回值说明、前置条件、支持场景、注意事项、调用路径、规格矩阵、地址来源规格等任何其他内容。

### CK6_CHECK

**spec.md 域表**：域数以 knowledge.md §4 为准 / 域目录路径正确 / API 数正确

**每 API 10 项**：函数原型 / 函数功能 / 输入说明(含规格矩阵) / 地址来源规格(仅有地址参数API) / 输出说明 / 返回值穷举 / 前置条件 / 支持场景 / 注意事项 / 调用路径到子模块边界

**伪造检查**：函数名/行号/签名/返回值/规格矩阵/支持场景/注意事项 — **全部基于源码，非头文件注释推断**

**格式**：0 `[待验证]`（只允许 `[未验证]`）/ file:line 标注 / 子模块引用一致性 / 0 自检标记

### CK7_REVERSE_OPTIMIZE

详见 [ck7-reverse-optimize-spec.md](references/ck7-reverse-optimize-spec.md)。

> **API skill 不直接修正子模块文档。** CK7 委托 driver-design-md-generate skill，逐项独立验证（重新 Read 子模块源码），确认后修正，驳回反馈。

## 质量约束

- 规格矩阵：每个复杂参数 API 必须有约束矩阵，覆盖所有校验分支
- **地址来源规格**：**仅有实际地址参数（ptr/src_ptr/va/pp 等）的 API 才需要**，描述地址的实际来源（`halMemAlloc`/`malloc`/`mmap`/共享池 VA 等）和要求。非地址参数（devid/flag/type/handle/shareable_handle/pool 等）的 API **可省略此节**，禁止机械添加
- **前置条件与依赖**：每个 API 须描述调用前必须满足的条件和依赖的接口（如 `halMemAgentOpen`/`halMemAlloc` 等）
- **函数功能**：基于源码 `switch`/`if` 分支分析，体现 API 的核心行为（如双向注册/路径分发等），非简略概述
- 返回值：穷举所有 return 路径，禁止只列成功+"其他错误"
- 注意事项：**基于源码校验逻辑，禁止从头文件 @attention 复制**
- file:line：所有函数引用标注 `{文件名}:{行号}`
- 0 `[待验证]`：只允许 `[未验证]`
- 调用路径：到子模块边界
- Device 侧 API（DV_ONLINE）：标注 `[未验证]`

## 触发词

"生成 api 规格文档" / "api spec" / "生成 {API}_spec.md" / "接口规格" / "生成 {domain} 域 spec" / "刷新 api spec"

## 引用文档

| 文档 | 内容 | 何时读 |
|------|------|--------|
| [api-master-template.md](references/api-master-template.md) | per-API spec 模板 + 可选 domain 模板 | CK4 生成前 |
| [api-spec-matrix-guide.md](references/api-spec-matrix-guide.md) | 规格矩阵方法论+alloc 域实例 | CK1V_SCAN L4 时 |
| [ck7-reverse-optimize-spec.md](references/ck7-reverse-optimize-spec.md) | 反向优化详细规范+清单格式+委托流程 | CK7 执行时 |
| [experience-notes.md](references/experience-notes.md) | alloc 域生成经验教训 | 参考 |
