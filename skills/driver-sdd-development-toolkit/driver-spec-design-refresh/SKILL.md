---
name: driver-spec-design-refresh
description: 通用模块文档增量刷新技能。自动检测代码变更，定位影响的文档范围（子模块/域/顶层），调用对应 skill 刷新文档，执行反向优化闭环。适用于代码 MR 合入后的文档跟随刷新。配合 per-module knowledge.md 使用。
---

# 模块文档增量刷新

## 概览

代码变更后，已生成的模块文档体系需要跟随刷新。本 skill 自动完成以下闭环：

```text

1. 检测变更    ← 用 git diff 检测哪些 .c/.h 文件变了

     ↓

1. 定位影响    ← 变更文件 → 影响的子模块文档 → 影响的域文档 → 影响的顶层文档

     ↓

1. 刷新文档    ← 调用 driver-design-md-generate / driver-spec-md-generate 刷新受影响文档

     ↓

1. 反向优化    ← 检查域文档的反向优化建议清单，反哺到子模块文档

     ↓

1. 更新索引    ← 更新 spec.md 索引和 design.md 子模块清单

```

**前置条件**：

- 文档已生成在 `.reverse_analysis/output/{module}/{ver}/` 下
- `skills/driver-design-md-generate/` 和 `skills/driver-spec-md-generate/` skill 存在

**参考文件**：

- knowledge.md §5 — 变更文件→影响文档范围映射矩阵（per-module）
- 代码仓 `driver-design-md-generate/SKILL.md` — 子模块文档生成 skill（全量）
- 代码仓 `driver-spec-md-generate/SKILL.md` — API 域文档生成 skill（全量）

---

## 第零步：知识加载 [CK0_KNOWLEDGE]

> 加载 knowledge.md 获取模块源码范围（§2）、API 域划分（§4）、变更文件→文档映射矩阵（§5）。
> knowledge.md 的生成流程见 `skills/driver-design-md-generate/references/knowledge-template.md` 的 CK0_KNOWLEDGE 步骤。
> 如 knowledge.md 不存在，先执行 driver-design-md-generate 的 CK0 引导生成流程。

## 第一步：检测变更

### 1.1 用 git diff 检测

```bash

# 检测自上次文档生成后的变更
# 注意：必须包含 pkg_inc/，因为公共 API 声明在此目录（新增/删除/签名变更会影响 spec.md 和域文档）
git diff --name-only <last_doc_commit> HEAD -- "{hal_src_dir}/" "{sdk_src_dir}/" "pkg_inc/"

```

产出变更文件列表，包含：

- 新增/修改/删除的 `.c`/`.h` 文件

# 检测自上次文档生成后的变更
# 注意：必须包含 pkg_inc/，因为公共 API 声明在此目录（新增/删除/签名变更会影响 spec.md 和域文档）

git diff --name-only \<last_doc_commit> HEAD -- "{hal_src_dir}/" "{sdk_src_dir}/" "pkg_inc/"

```

### 1.3 变更分类

将变更文件按类型分类。通用分类规则：

| 文件类型 | 分类 | 影响范围 |
|---------|------|---------|
| `{hal_src_dir}/api/master/*.c` | API 层变更 | 域文档 + spec.md |
| `{hal_src_dir}/api/master/*.h` | API 头文件变更 | 域文档 + spec.md + AGENTS.md |
| `pkg_inc/*.h` | 公共头文件变更 | 相关域文档 + AGENTS.md |
| `{hal_src_dir}/{submodule}/*.c` 或 `{sdk_src_dir}/{submodule}/*.c` | 子模块变更 | 子模块 design.md |
| `command/msg/*.h` | 消息定义变更 | AGENTS.md + 相关子模块 |
| `inc/*.h` | 内部头文件变更 | 相关子模块 design.md |
| `framework/*.c` | 框架变更 | framework_design.md + AGENTS.md |
| 新增目录/文件 | 新子模块 | design.md + AGENTS.md + 新子模块 design.md |

> 模块特有路径映射见 knowledge.md §5。

---

## 第二步：定位影响

### 2.1 变更文件 → 子模块文档映射

根据变更文件的路径，定位到子模块文档。完整映射矩阵读取 **knowledge.md §5**。

AI 基于目录规则推导映射关系，跨目录映射需用户在 knowledge.md §5 中确认。

### 2.2 子模块 → 域文档映射

根据 knowledge.md §4 的 API 域划分，判断哪些域文档受影响。

| 判断条件 | 影响域文档 |
|---------|-----------|
| API 层文件变更 | 该文件所属域的 domain_design.md |
| 子模块接口变更 | 引用该子模块的域文档 |
| API 新增/删除 | spec.md + 对应域文档 |

> 域归属见 knowledge.md §4。

### 2.3 影响范围分层

```text

Layer 1: 子模块文档（直接受影响）
  ├── 变更文件对应的子模块 design.md
  └── §3.1 依赖该子模块的其他子模块（间接影响，需检查）

Layer 2: 域文档（API 行为变更时受影响）
  ├── 变更文件所属域的 domain_design.md
  └── §2 接口总览 + §3 API 规格

Layer 3: 顶层文档（接口增减/架构变更时受影响）
  ├── spec.md — API 索引表
  ├── design.md — §3.1 分层架构 + §4 子模块清单
  └── AGENTS.md — §2 目录约定（新增目录时）

```

### 2.4 输出影响范围清单

编排者输出一个影响范围清单表：

| 变更文件 | 变更类型 | 影响子模块文档 | 影响域文档 | 影响顶层文档 |
|---------|---------|--------------|-----------|------------|
| {module}_alloc.c | 修改 | malloc_mng_design.md | alloc_domain_design.md | — |
| {module}_vmm.c | 修改 | {submodule}_design.md | vmm_handle_domain + alloc_domain | — |
| 新增 {module}_xxx.c | 新增 | （新建）xxx_design.md | spec.md + design.md §4 | +AGENTS.md §2 |

---

## 第三步：刷新文档

### 3.1 子模块文档刷新

对每个受影响的子模块，调用 `driver-design-md-generate` skill 重新生成：

```text

分派 sub-agent（category="unspecified-high", run_in_background=true）:
  prompt:
    TASK: 刷新 {module}_design.md（增量变更）
    CONTEXT:

      - 变更文件: {changed_files}
      - 变更类型: {修改/新增}
      - 子模块文档路径: {doc_path}

    STEP 0: READ SKILL FILES

      1. .codeartsdoer/skills/driver-design-md-generate/SKILL.md
      2. .codeartsdoer/skills/driver-design-md-generate/references/design_template.md

    MUST DO:

      - 读取变更的 .c 文件，确认变更内容
      - 读取现有 design.md，保留未变更部分
      - 只刷新受变更影响的章节（§2 接口/§3 依赖/§5 结构体/§6 设计要点）
      - 执行 CK1V_SCAN 验证变更部分
      - 更新变更记录，标注 "增量刷新: {变更描述}"

    MUST NOT DO:

      - 不要全量重写（只刷新受影响章节）
      - 不要删除未变更内容

```

**并行刷新**：多个子模块文档可并行刷新（独立 sub-agent）。

### 3.2 域文档刷新

对每个受影响的域文档，调用 `driver-spec-md-generate` skill 刷新：

```text

分派 sub-agent（category="unspecified-high", run_in_background=true）:
  prompt:
    TASK: 刷新 {domain}_domain_design.md（增量变更）
    CONTEXT:

      - 变更文件: {changed_files}
      - 域文档路径: {doc_path}
      - 变更的 API: {changed_apis}（如果有）

    STEP 0: READ SKILL FILES

      1. .codeartsdoer/skills/driver-spec-md-generate/SKILL.md
      2. .codeartsdoer/skills/driver-spec-md-generate/references/api-master-template.md

    MUST DO:

      - 读取现有域文档，保留未变更部分
      - 只刷新受变更影响的 §3 API 规格（如果有 API 签名/行为变更）
      - 更新 §2 接口总览（如果有新增/删除 API）
      - 更新 §6 文件索引（如果有新增文件）
      - 执行 CK1V_SCAN 验证变更部分
      - 更新变更记录

    MUST NOT DO:

      - 不要全量重写
      - 不要删除未变更的 API 规格

```

### 3.3 顶层文档刷新

| 顶层文档 | 刷新条件 | 刷新内容 |
|---------|---------|---------|
| spec.md | 新增/删除 API | §3 索引表增删行 |
| spec.md | API 功能简述变更 | §3 对应行更新 |
| design.md | 新增子模块 | §4 子模块清单增行 |
| design.md | 分层架构变更 | §3.1 架构图更新 |
| AGENTS.md | 新增目录 | §2 目录约定增行 |
| AGENTS.md | 新增消息结构体 | §5 消息结构体规则 |
| AGENTS.md | 新增历史坑点 | §6 历史坑点增条目 |

---

## 第四步：反向优化

### 4.1 检查域文档的反向优化建议清单

域文档刷新后，检查"反向优化建议清单"是否有新增项：

```text

1. 读取刷新后的域文档的"反向优化建议清单"
2. 对比刷新前的清单，找出新增项
3. 新增项指向子模块文档的补充/修正

```

### 4.2 反哺到子模块文档

对新增的反向优化项，分派 sub-agent 补充到子模块文档：

```text

分派 sub-agent（category="unspecified-high"）:
  prompt:
    TASK: 应用 {domain}_domain_design.md 反向优化建议 #{n} 到 {module}_design.md
    CONTEXT:

      - 反向优化建议: {建议内容}
      - 源码依据: {file:line}
      - 子模块文档路径: {doc_path}

    MUST DO:

      - 读取源码验证建议准确性
      - 在子模块文档对应章节补充内容
      - 更新变更记录

```

---

## 第五步：更新索引

### 5.1 更新 spec.md

- 如果有 API 新增/删除：更新 §3 索引表（增删行）
- 如果有 API 功能简述变更：更新 §3 对应行
- 更新 §2 域总览表的 API 数量
- 更新变更记录

### 5.2 更新 design.md

- 如果有新增子模块：更新 §4 子模块清单表
- 如果有架构变更：更新 §3.1 分层架构图
- 更新变更记录

### 5.3 更新 AGENTS.md

- 如果有新增目录：更新 §2 目录约定表
- 如果有新增消息结构体：更新 §5 消息结构体规则
- 如果发现新坑点：更新 §6 历史坑点

---

## 刷新频率指南

| 变更类型 | 刷新范围 | 频率 |
|---------|---------|------|
| 修改函数实现（不改签名） | 子模块 §6 设计要点 | 随 MR |
| 新增/修改 API 签名 | 域文档 + spec.md | 随 MR |
| 新增子模块目录 | design.md §4 + AGENTS.md §2 + 新子模块 design.md | 随 MR |
| 新增消息结构体 | AGENTS.md §5 | 随 MR |
| 架构调整 | 全量刷新 | 版本发布时 |
| 仅注释/格式变更 | 不刷新 | — |

---

## Sub-Agent 调用模板

分派 sub-agent 刷新文档时，prompt 包含：TASK（增量刷新 {module}_design.md）、STEP 0 READ SKILL FILES（driver-design-md-generate/SKILL.md + design_template.md）、CONTEXT（变更文件/类型/文档路径）、MUST DO（读变更 .c + 保留未变更部分 + 只刷新受影响章节 + CK1V_SCAN 验证 + 更新变更记录）、MUST NOT DO（不全量重写 + 不删除未变更内容）。
