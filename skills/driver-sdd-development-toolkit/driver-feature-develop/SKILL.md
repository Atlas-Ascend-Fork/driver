---
name: driver-feature-develop
description: 通用模块新需求分析技能。基于已生成的文档体系（spec.md/design.md/驱动级AGENTS.md/接口规格文档/子模块文档），分析新需求的影响范围、复用现有接口、确认约束边界，按需求设计模板生成实现说明书。配合 per-module knowledge.md 使用。适用于任意模块的新功能开发、接口扩展、缺陷修复方案设计。
---

# 模块新需求分析

## 概览

**目标**：利用已有文档体系快速理解现状，确认约束边界，避免重复造轮子，按模板生成完整需求设计说明书。

```text

第1步：理解需求
  ↓
第2步：CK_DOC_GATE 文档先行 → CK_VERIFY 复用验证
  ↓
第3步：约束检查 → 第4步：影响范围
  ↓
第5步：生成需求设计说明书（Requirement_Design.md）
  ↓
⏸️ 等待用户确认 ← 用户审查设计文档合理性，确认无问题后继续
  ↓
第6步：生成实现级 design.md 和 spec.md
  ↓
⏸️ 等待用户确认 ← 用户审查实现级文档，确认无问题后继续
  ↓
第7步：编码实现
  ↓
CK_CODE_GATE → CK_CODE_COMPILE → CK_CODE_REVIEW

```

**文档体系**：

| 文档 | 路径 | 用途 |
|------|------|------|
| spec.md | `output/{module}/{ver}/spec.md` | API 域索引 |
| 接口规格 | `output/{module}/{ver}/1_support/api_master/api/{domain}/{API}_spec.md` | 接口完整规格 |
| design.md | `output/{module}/{ver}/design.md` | 模块设计 |
| AGENTS.md | `output/AGENTS.md` | 驱动公共规范 |
| 子模块文档 | `output/{module}/{ver}/{category}/{submodule}/{submodule}_design.md` | 子模块详细设计 |

**参考文件**：

- [`references/Requirement_Design_Template.md`](references/Requirement_Design_Template.md) — 需求设计说明书模板
- [`references/impl_design_template.md`](references/impl_design_template.md) — 实现级 design.md 模板
- [`references/impl_spec_template.md`](references/impl_spec_template.md) — 实现级 spec.md 模板
- [`references/req-doc-usage.md`](references/req-doc-usage.md) — 文档使用方法
- [`references/doc_gate_rules.md`](references/doc_gate_rules.md) — CK_DOC_GATE 补充规则（交叉引用跟进+调用链模块全覆盖）
- [`references/ck_verify_rules.md`](references/ck_verify_rules.md) — CK_VERIFY 检查点 L1-L9 详细规则+示例
- [`references/ck_code_rules.md`](references/ck_code_rules.md) — CK_CODE 编码前/后检查规则+review sub-agent 模板

---

## 第0步：知识加载 [CK0_KNOWLEDGE]

> 加载 knowledge.md 获取模块定位（§1）、源码范围（§2）、子模块清单（§3，含消息前后处理文件名）、API 域划分（§4）。
> knowledge.md 的生成流程见 `skills/driver-design-md-generate/references/knowledge-template.md` 的 CK0_KNOWLEDGE 步骤。
> 如 knowledge.md 不存在，先执行 driver-design-md-generate 的 CK0 引导生成流程。

## 第1步：理解需求

提取需求维度（类型/涉及能力/用户态内核态/芯片型号/对外接口/数据结构）+ 关键词（API名/功能词/模块名/结构体名）。

---

## 第2步：CK_DOC_GATE 文档先行门控（强制）

> **在搜索任何源码之前，必须完成此门控。** 详见 [`references/doc_gate_rules.md`](references/doc_gate_rules.md)。

门控流程：

1. **先读文档**：按 §2.1-§2.5 顺序读相关文档
2. **记录文档结论**：记录"找到了什么 / 没找到什么"
3. **文档未覆盖的部分才搜索代码**

**补充规则（强制，详见 doc_gate_rules.md）**：

- **规则1**：交叉引用必须跟进读，不能当黑盒（smm 提到 pmq ops → 必须读 pmq_design.md）
- **规则2**：调用链上的模块文档都要读，不只读"感觉相关"的（判断标准=是否在调用链上）

### 2.1 查 spec.md → 确认可复用 API

### 2.2 查接口规格文档 → 理解调用路径和约束

### 2.3 查子模块文档 → 理解设计意图和依赖（§2 对外接口/§5 结构体/§6 设计要点/§3 依赖）

### 2.4 查 design.md → 确认架构分层（§4.1 分层/§5 子模块清单/§4.2 结构体）

### 2.5 查 AGENTS.md → 确认部署和消息通道

> **注意**：子模块文档 §2 可能不完整，以头文件为准，记录文档缺陷。文档不完整不等于跳过文档。

---

## CK_VERIFY 复用验证检查点

> **源码是唯一验证标准。** 详见 [`references/ck_verify_rules.md`](references/ck_verify_rules.md)。

| 检查点 | 触发条件 | 一句话 |
|--------|---------|--------|
| L1-L3 | 声明复用函数时 | 确认存在性+签名+返回路径 |
| L4 | 声明复用函数时 | 列出关键假设，逐个源码验证 |
| L5 | 从结构体找区分字段时 | 一次性列全部字段，禁止逐个试错 |
| L6 | 新增/修改消息时 | grep 模块的消息前后处理文件（文件名从 knowledge.md §3 获取）→ 检查 pre/post handlers 适配；若 knowledge.md 未提供则跳过 L6 |
| L7 | 跨模块通信时 | 画完整调用链 → 逐层检查约束 → **平行路径（local/remote/send/receive）逐一验证，不能假设对称** |
| L8 | 函数源码不在本仓时 | 从参数语义+已有代码能工作+交叉引用文档推断，禁止凭空假设；源码也没有→告知人工 |
| L9 | 简化/修改数据结构时 | 列出所有消费者（发送方+接收方+中间层+传输层），逐个确认格式影响 |
| L10 | 替换/重写函数时 | 逐行对比原始函数所有副作用（状态更新/锁/日志）+ 调用链上下游是否依赖这些副作用 |

---

## 第3步：约束检查

> 利用 AGENTS.md + design.md 确认约束边界。

| 检查项 | 查哪里 | 判定 |
|--------|--------|------|
| 职责范围 | design.md §1.1 | ✅ 内存管理 / ❌ 越界 |
| 跨模块通信 | 追踪模块→esched/msg_chan/ioctl 传输层 | ⚠️ 详见 CK_VERIFY L7 |
| 新组件依赖 | design.md §2 | ⚠️ 需人工确认 |
| 目录约定 | design.md §2.1 | 用户态 api/master/ 或 assign/ 等 |
| 编码规范 | AGENTS.md §3.1-§3.5 | 错误处理/日志/资源清理/include 顺序/注释英文 |
| 消息相关逻辑 | grep 模块的消息前后处理文件（见 knowledge.md §3） | ⚠️ 详见 CK_VERIFY L6 |
| 禁区 | AGENTS.md §4 | ❌ 不改已发布接口/消息ID |
| 人工确认项 | AGENTS.md §4.3 | 消息结构体/适配层/错误码 |

### §3.5 失败语义一致性检查（强制）

> **新增/优化路径的失败处理必须与已有路径语义一致。** 先追踪已有路径失败时做了什么（回滚？延迟清理？直接返回？），新路径必须对齐。

| 检查项 | 查哪里 | 判定 |
|--------|--------|------|
| 已有路径失败时做了什么？ | 源码追踪已有路径失败分支 | 记录失败语义 |
| 新路径失败处理是否一致？ | 新设计文档 §2.4.1 | 必须对齐 |

---

## 第4步：影响范围分析

利用 design.md §4.1 分层架构，确认：

- 新代码在哪一层？依赖哪些下层？被哪些上层调用？
- 模块影响清单 + 文档影响清单

---

## 第5步：生成需求设计说明书

> 按 [`references/Requirement_Design_Template.md`](references/Requirement_Design_Template.md) 填充，可分派 sub-agent 辅助。

### 5.1 读取模板 → 按 10 章+3 附录填充

### 5.2 文档辅助填充映射表

| 模板章节 | 数据来源 |
|---------|---------|
| §1.2 接口清单 | spec.md §2 |
| §2.1 需求输入 | 子模块 §3.1 依赖 |
| §2.2 处理流程 | 接口规格 调用路径 |
| §2.4 场景分析 | 接口规格 约束 |
| §2.4.1 已有路径失败语义 | 源码追踪已有路径失败分支 |
| §3.1 结构体 | 子模块 §5 |
| §3.3 文件变更 | design.md §2.1 |
| §3.4 内部结构 | design.md §4.1 |
| §4.1 影响分析 | design.md §4.1 |
| §5 对外接口 | spec.md §2 + 接口规格 |
| §8 变更影响 | AGENTS.md §2+§3 |
| 附录A 代码引用 | 各文档 file:line |

### 5.3 分派 sub-agent 验证

```text

task(category="unspecified-high", run_in_background=true, prompt="""
  ## TASK: 验证设计文档中的代码引用和调用链
  ## DOC_REFS: spec.md / {API}_spec.md / {module}_design.md / AGENTS.md
  ## GOAL: 验证调用链 / 确认复用接口 / 检查约束 / 确认未触禁区
  ## MUST DO: 先读文档再搜索代码，所有引用标注 file:line
""")

```

### 5.4 输出路径约束（强制）

所有生成的文档**必须**放在 `{requirement_name}_feature/` 目录下，统一归档：

| 文档 | 路径（.reverse_analysis 工作副本） | 路径（docs/module git 副本） |
|------|--------------------------------|---------------------------|
| 需求设计说明书 | `.reverse_analysis/output/{module}/{ver}/{requirement_name}_feature/Requirement_Design.md` | `docs/zh/module/{module}/{ver}/{requirement_name}_feature/Requirement_Design.md` |
| 实现级 design.md | `.reverse_analysis/output/{module}/{ver}/{requirement_name}_feature/design.md` | `docs/zh/module/{module}/{ver}/{requirement_name}_feature/design.md` |
| 实现级 spec.md | `.reverse_analysis/output/{module}/{ver}/{requirement_name}_feature/spec.md` | `docs/zh/module/{module}/{ver}/{requirement_name}_feature/spec.md` |

**禁止**：

- ❌ 需求设计说明书放在模块根目录（如 `{module}_{ver}/{requirement_name}_Requirement_Design.md`）
- ❌ design.md/spec.md 放在 `1_support/api_master/{requirement_name}/` 子目录
- ✅ 统一放在 `{requirement_name}_feature/` 目录

生成后同步：将 `.reverse_analysis/output/{module}/{ver}/{requirement_name}_feature/` 复制到 `docs/zh/module/{module}/{ver}/{requirement_name}_feature/`

### ⏸️ 用户确认门控 1（强制）

> **生成需求设计说明书后，必须停下来等用户确认。** 不要自动继续生成实现级文档。

向用户输出：
> 需求设计说明书已生成：`{path}`。请审查以下关键点：
>

> 1. 数据结构设计是否合理？
> 2. 处理流程是否正确？
> 3. 失败语义是否与已有路径一致？
> 4. 影响范围是否完整？
> 5. 有无遗漏的约束/禁区？

>
> 确认无问题后回复"继续"，我将生成实现级 design.md 和 spec.md。

**禁止**：用户未确认就自动生成实现级文档。

### 5.5 生成实现级 design.md 和 spec.md

> **用户确认需求设计说明书后，才执行此步。**

按以下模板填充实现级文档：

**design.md**（精确实现方案，指导 AI 编码）：

- 模板见 [`references/impl_design_template.md`](references/impl_design_template.md)
- 定位：结合需求设计文档 + 存量 design.md/spec.md + 实际源码，给出精确的代码实现方案
- §1 变更概述（变更点清单 + 设计约束来源）
- §2 变更点详细设计（按变更点组织，每个变更点含全部编码要素）
- §3 编码约束确认（CK_CODE_GATE 6 项）
- §4 Device 侧实现指导（如有）
- §5 文件索引
- 核心要求：修改类变更给修改前→修改后对比；伪码接近实际 C 代码；插入点精确到 file:line + 上下文锚点

**spec.md**（功能规格说明）：

- 模板见 [`references/impl_spec_template.md`](references/impl_spec_template.md)
- 定位：明确功能点、支持/不支持场景、接口详细规格
- §1 功能点清单
- §2 接口规格（参考存量 spec.md 格式）
- §3 支持场景 + §4 不支持场景
- §5 复用接口补充说明
- §6 消息规格（如有）
- §7 文件索引

**编码要素**（design.md §2 每个变更点必须包含，缺任何一个 AI 无法直接编码）：

1. 插入点与上下文锚点（文件:行号 + 上下文描述 + 周围代码）
2. 修改前→修改后对比（修改类）或新增代码（新增类）
3. 结构体/枚举定义（如有）
4. include 依赖
5. 核心实现逻辑（接近实际 C 代码的伪码）

### ⏸️ 用户确认门控 2（强制）

> **生成实现级 design.md 和 spec.md 后，必须停下来等用户确认。** 不要自动开始编码。

向用户输出：
> 实现级文档已生成：`{design.md path}` + `{spec.md path}`。请审查：
>

> 1. 每个变更点的编码要素是否完整（插入点/对比/伪码/include/结构体）？
> 2. 修改前→修改后对比是否精确到行？
> 3. device 侧实现指导是否充分（如有）？
> 4. 接口规格是否覆盖所有参数/返回码/约束？
> 5. 支持/不支持场景是否明确？

>
> 确认无问题后回复"继续"，我将开始编码实现。

**禁止**：用户未确认就自动开始编码。

---

## 第7步：编码实现

> **用户确认实现级文档后，才执行此步。**

按 design.md 和 spec.md 中的 5 个编码要素实现代码。

> 详见 [`references/ck_code_rules.md`](references/ck_code_rules.md)。

- **CK_CODE_GATE**：写代码前逐项确认约束（注释英文/类型/include 顺序/日志/错误处理/资源清理）
- **CK_CODE_DATAFLOW**：编码时每行新代码执行数据流三步验证（① 溯源：值从哪来，读赋值表达式确认精确值 → ② 追跳：经什么转换，逐层追踪类型和值变化 → ③ 代入：用具体数值代入运算，确认边界场景结果正确）
- **CK_CODE_COMPILE**：编码后执行编译检查（`build.sh` / `cmake` / `make`），0 error 0 warning
- **CK_CODE_REVIEW**：编码后分派 review sub-agent 检查（加载三层规则→逐项检视→全✅才提交 PR）
- **CK_CODE_SUBMIT**：提交 PR 前必须执行 code-submit skill（rebase 到最新 upstream/master → diff 验证只含目标变更无残留 → 编码约束检查 → 逐文件暂存禁止 `git add -A` → commit + push）

> **禁止跳过 CK_CODE_SUBMIT**：即使 CK_CODE_REVIEW 全 ✅，也必须执行 code-submit skill 的第1-4步完整流程后才能 push。跳过会导致 PR 混入无关文件（如 `.pre-commit-config.yaml`、`.codeartsdoer/`、`.reverse_analysis/` 等）。

---

## Sub-Agent 调用模板

| 场景 | 触发条件 | 做什么 |
|------|---------|--------|
| 文档查询 | 需读 5+ 模块 design.md | 并行读文档、汇总结论 |
| CK_VERIFY 深度验证 | 调用链跨 3+ 模块 | 画调用链、逐层检查约束、输出结论 |
| 设计文档验证 | 设计文档完成 | 验证代码引用和调用链 |
| CK_CODE_REVIEW | 编码完成 | 加载规则检视代码（详见 ck_code_rules.md） |
