# knowledge.md 模板与引导生成流程

> 本文件定义 per-module knowledge.md 的模板结构，以及 AI 扫描 + 用户确认的 CK0_KNOWLEDGE 引导生成流程。
> 供 driver-design-md-generate / driver-spec-md-generate / driver-spec-design-refresh / driver-feature-develop 加载。

---

## 1. knowledge.md 模板结构

每个模块需生成一份 `{module}-knowledge.md`，结构如下：

```markdown

# {模块名} Knowledge

> 本文件由 AI 扫描 + 用户确认生成，供 driver-design-md-generate / driver-spec-md-generate /
> driver-spec-design-refresh / driver-feature-develop 加载。
> 生成流程见 `skills/driver-design-md-generate/references/knowledge-template.md`。

---

## §1 模块定位

| 项 | 值 |
|----|----|
| 模块名 | {如 my_module} |
| 模块版本 | {如 v1} |
| 软件栈位置 | {如 HAL 功能模块层 + SDK Driver 功能模块层} |
| 构建产物 | {如 HAL→libascend_hal.so, SDK→.ko} |
| 设备节点 | {如 /dev/my_module, /dev/my_module_mmap} |
| 硬件支持范围 | {如 UVM 仅 910B/910_93} |

## §2 源码范围（用户指定）

> ⚠️ 必须由用户确认。AI 可基于目录结构给出建议范围，但最终以用户指定的为准。

| 侧 | 路径 | 文件数 | 子目录数 |
|----|------|--------|---------|
| HAL 用户态 | `src/ascend_hal/{module}/{ver}/` | {n} | {n} |
| SDK 内核态 | `src/sdk_driver/{module}/{ver}/` | {n} | {n} |

## §3 子模块清单（AI 扫描 + 用户确认）

> ⚠️ AI 基于源码目录结构自动扫描，生成候选清单。**用户必须确认划分合理性**。
> 提醒：若某子模块过大（建议 >30 文件或逻辑混杂），需要进一步切分。

### HAL 用户态子模块

| 子目录 | 功能（AI 从 README/头文件提取，用户可改） | 文件数 |
|--------|----------------------------------------|--------|
| api/master/ | {AI 填写} | {n} |
| ... | ... | ... |

### SDK 内核态子模块

| 子目录 | 功能 | 阶段 | 文件数 |
|--------|------|------|--------|
| framework/ | {AI 填写} | Stage 7 | {n} |
| ... | ... | ... | ... |

## §4 API 域划分（AI 扫描 + 用户确认）

> ⚠️ AI 从头文件提取对外 API 函数列表，给出域归属建议。**用户必须确认域划分**。

| 域 | 含 API（函数名） | 主实现文件 |
|----|-----------------|-----------|
| alloc | halMemAlloc, halMemFree, ... | api/master/{module}_alloc.c |
| ... | ... | ... |

## §5 变更文件→文档映射矩阵

> 用于 driver-spec-design-refresh。AI 基于目录规则推导，用户确认跨目录映射。

| 变更文件路径 | 影响子模块文档 | 影响域文档 | 影响顶层文档 |
|--------------|--------------|-----------|------------|
| api/master/{module}_alloc.c | {module}_malloc_design.md | alloc_domain_design.md | — |
| ... | ... | ... | ... |

## §6 参考文档索引（可选）

> 模块外部已有的参考文档。如无填"无"。

| 路径 | 内容 |
|------|------|
| {reference_docs_dir}/kernel/xxx.md | 模块分类/依赖关系图 |
| ... | ... |

## §7 质量约束（模块规模相关）

| 项 | 值 |
|----|----|
| 文件总数 | {n} |
| 建议文档行数下限 | {如 800（大模块）/ 300（中模块）/ 150（小模块）} |
| 建议最小 Mermaid 图数 | {如 3（大模块）/ 1（小模块）} |
| API 域数 | {n} |
| API 总数 | {n} |

```

---

## 2. CK0_KNOWLEDGE 引导生成流程

所有通用 skill 在 CK1_INIT 之前增加 **CK0_KNOWLEDGE** 前置步骤：

```text

CK0_KNOWLEDGE（知识加载/引导生成）
  ├─ 检测 knowledge.md 是否存在
  │   ├─ 存在且有效 → 加载，进入 CK1_INIT
  │   └─ 不存在 → 进入引导生成流程 ↓
  │
  └─ 引导生成流程（3 轮交互）：
     ├─ 第1轮：用户指定代码范围
     │   AI 请求用户指定：
      │     1) HAL 用户态路径（如 src/ascend_hal/{module}/{ver}/）
      │     2) SDK 内核态路径（如 src/sdk_driver/{module}/{ver}/）
      │     3) 模块名 + 版本（如 my_module v1）
     │   → AI Glob 扫描两路径，统计文件数/子目录数，填入 §1/§2
     │   ⚠️ 代码范围必须由用户指定，AI 只提供建议
     │
     ├─ 第2轮：AI 扫描子模块 + 用户确认划分
     │   AI 扫描源码目录结构 → 提取子目录清单
     │   → 从每个子目录的 README/头文件提取功能说明
     │   → 填入 §3 候选清单
     │   → 输出给用户确认：
     │     "以下子模块划分是否合理？
     │      ⚠️ 提醒：若某子模块过大（>30 文件或逻辑混杂），建议进一步切分。"
     │   → 用户确认/修改 → 写入 §3
     │   ⚠️ 子模块划分 AI 可自行扫描，但最终需用户确认合理性
     │
     └─ 第3轮：AI 扫描 API + 用户确认域划分
         AI 从 pkg_inc/ 对外头文件提取 DLLEXPORT 函数
         → 给出域归属建议（按函数名前缀/实现文件聚类）
         → 输出给用户确认：
           "以下 API 域划分是否合理？
            ⚠️ 提醒：请确认域划分，跨域 API 需注明归属。"
         → 用户确认/修改 → 写入 §4/§5
         ⚠️ API 域需用户划分
  │
  → knowledge.md 生成完成，保存到 {output_dir}/knowledge.md
  → 进入 CK1_INIT

```

---

## 3. 三点提醒落点

| 提醒 | 落点 |
|------|------|
| ① 代码范围需要用户指定 | CK0 第1轮交互 + knowledge.md §2 标注"⚠️ 必须由用户确认" |
| ② 子模块划分 AI 扫描但需用户确认，过大需切分 | CK0 第2轮交互 + knowledge.md §3 标注"⚠️ 用户必须确认" + 本文件切分提醒 |
| ③ API 域需用户划分 | CK0 第3轮交互 + knowledge.md §4 标注"⚠️ 用户必须确认" |
