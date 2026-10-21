# Driver Doc Toolkit

> CANN Driver 通用模块文档工具包——为任意模块生成、刷新、需求分析文档，配合 per-module `knowledge.md` 使用。

---

## 1. 工具包组成

```text

skills/driver-sdd-development-toolkit/
├── README.md                          ← 本文件
├── driver-design-md-generate          ← Skill 1：子模块文档生成
├── driver-spec-md-generate            ← Skill 2：API 域规格文档生成
├── driver-spec-design-refresh         ← Skill 3：增量刷新
└── driver-feature-develop             ← Skill 4：需求分析 + 编码检查

```

---

## 2. 四个 Skill 的关系

```text

                        ┌─────────────────────────────┐
                        │     knowledge.md (per-module) │
                        │  §1 模块定位  §2 源码范围      │
                        │  §3 子模块清单  §4 API 域      │
                        │  §5 变更映射  §6 参考文档      │
                        │  §7 质量约束                  │
                        └──────────┬──────────────────┘
                                   │ CK0 加载
             ┌─────────────────────┼─────────────────────┐
             │                     │                     │
     ┌───────▼────────┐   ┌────────▼────────┐   ┌───────▼────────┐
     │ driver-design-  │   │driver-spec-md-   │   │driver-feature- │
     │ md-generate     │   │generate          │   │develop         │
     │  子模块 design.md│   │ API spec.md      │   │ 需求设计说明书  │
     │  + 顶层 design.md│   │ + 域 design.md   │   │                │
     └───────┬────────┘   └────────┬────────┘   └───────┬────────┘
             │                     │                     │
             │    ┌────────────────┘                     │
             ▼    ▼                                      ▼
     ┌─────────────────┐                        ┌────────────────┐
     │driver-spec-     │──代码变更──>刷新上述文档 │ CK_CODE_GATE   │
     │design-refresh   │                        │ CK_CODE_COMPILE│
     └─────────────────┘                        │ CK_CODE_REVIEW │
                                                  └────────────────┘

```

### 2.1 各组件职责

| 组件 | 职责 | 必需/可选 |
|------|------|----------|
| **knowledge.md** | 承载模块特有知识（源码范围/子模块/API域/映射矩阵）。由 AI 扫描 + 用户确认生成 | **必需** |
| **driver-design-md-generate** | 生成子模块 design.md + 顶层 design.md/spec.md/AGENTS.md（三层递进模板） | **核心** |
| **driver-spec-md-generate** | 生成逐 API spec.md（10 项规格）+ 域级 design.md | **核心** |
| **driver-spec-design-refresh** | 代码变更后增量刷新受影响文档 | 按需 |
| **driver-feature-develop** | 新需求分析 → 需求设计说明书 → 实现级文档 → 编码检查 | 按需 |

### 2.2 工作模式

skill 触发后，LLM 按 SKILL.md 指令执行：

```text

skill触发 → LLM自行 Grep/Glob 扫描源码 → Read 关键源码 → 最终文档

```

---

## 3. 使用指导

### 3.1 首次使用（新模块）

#### 第1步：在 OpenCode 中使用 skill

在 OpenCode 中输入：

```text

生成 hdc 模块文档

```

触发 `driver-design-md-generate` skill，它会：

1. **CK0_KNOWLEDGE**：检测 knowledge.md 不存在 → 进入 3 轮引导：
   - 第1轮：你指定代码范围（`src/ascend_hal/hdc` + `src/sdk_driver/hdc`）
   - 第2轮：AI 扫描子目录 → 你确认划分合理性（过大需切分）
   - 第3轮：AI 从 pkg_inc 提取 API → 你确认域划分
2. knowledge.md 生成完成
3. **CK1_INIT → CK1V_SCAN → CK4_MODULE**：按三层递进模板生成 design.md

### 3.2 日常使用

| 场景 | 触发方式 | 产出 |
|------|---------|------|
| 生成模块文档 | "生成 {module} 文档" | design.md / spec.md / AGENTS.md |
| 生成 API 规格 | "生成 {module} api spec" | 逐 API {API}_spec.md + 域 design.md |
| 增量刷新 | 代码变更后 "刷新 {module} 文档" | 受影响文档更新 |
| 新需求分析 | "分析 {需求描述}" | Requirement_Design.md → design.md → spec.md → 编码 |

### 3.3 关键概念

#### knowledge.md

每个模块需要一份 `knowledge.md`，由 AI 扫描 + **用户确认**生成。包含 7 节：

| 节 | 内容 | 生成方式 |
|----|------|---------|
| §1 模块定位 | 模块名/版本/软件栈位置/构建产物/设备节点 | AI 从 README 提取 + 用户确认 |
| §2 源码范围 | HAL 路径 + SDK 路径 | **用户指定**（⚠️ 必须用户确认） |
| §3 子模块清单 | HAL 子目录 + SDK 子目录，按功能域/阶段分组 | AI 扫描 + **用户确认**（⚠️ 过大需切分） |
| §4 API 域划分 | 域名 + API 列表 + 主实现文件 | AI 从头文件提取 + **用户确认**（⚠️ 用户划分） |
| §5 变更文件→文档映射 | 变更文件路径 → 受影响文档 | AI 推导 + 用户确认跨目录映射 |
| §6 参考文档索引 | 外部参考文档路径 | 可选，如无填"无" |
| §7 质量约束 | 文件数/行数下限/Mermaid 图数/API 数 | 基于模块规模自动建议 |

#### CK0_KNOWLEDGE 三轮提醒

| 提醒 | 落点 |
|------|------|
| ① 代码范围需要用户指定 | CK0 第1轮 — AI 只提供建议，用户最终确认 |
| ② 子模块划分需用户确认，过大需切分 | CK0 第2轮 — >30 文件或逻辑混杂的子模块需进一步切分 |
| ③ API 域需用户划分 | CK0 第3轮 — 跨域 API 需注明归属 |

#### CK 检查点体系

| Skill | 检查点链 |
|-------|---------|
| driver-design-md-generate | CK0 → CK1_INIT → CK1V_SCAN(4层) → CK2_DECLARE → CK3_INTENT → CK4_MODULE → CK7_FORMAT → CK8_CHECK → CK10_REVERSE_REFRESH |
| driver-spec-md-generate | CK0 → CK1_INIT → CK1V_SCAN(4层) → CK4_API_SPEC → CK5_FORMAT → CK6_CHECK → CK7_REVERSE_OPTIMIZE |
| driver-spec-design-refresh | CK0 → 检测变更 → 定位影响 → 刷新文档 → 反向优化 → 更新索引 |
| driver-feature-develop | CK0 → CK_DOC_GATE → CK_VERIFY(L1-L10) → 约束检查 → 影响范围 → 需求设计 → ⏸️用户确认 → 实现文档 → ⏸️用户确认 → CK_CODE_GATE → CK_CODE_COMPILE → CK_CODE_REVIEW |

---

## 4. 完整工作链路

```text

需求分析 (driver-feature-develop)
  → 需求设计说明书 (Requirement_Design.md)
  → ⏸️ 用户确认
  → 实现级 design.md + spec.md
  → ⏸️ 用户确认
  → 编码 → CK_CODE_GATE → CK_CODE_COMPILE → CK_CODE_REVIEW
  → 提交 PR

文档生成 (driver-design-md-generate + driver-spec-md-generate)
  → CK0 生成 knowledge.md
  → 子模块 design.md + API 域 design.md + 顶层 spec.md/design.md/AGENTS.md

文档维护 (driver-spec-design-refresh)
  → 代码变更 → knowledge.md §5 映射 → 增量刷新受影响文档

```

---

## 5. 已有模块范例参考

本工具包从实际模块文档工作中沉淀而来。新模块可参考 zh/ 目录下已有的 skill 模板和 knowledge-template.md 结构来对照自己的产出。

---

## 6. 适配其他模块

1. **在 OpenCode 中触发 skill**：输入"生成 {module} 文档"
2. **CK0 引导生成 knowledge.md**：3 轮交互（指定范围 → 确认子模块 → 确认 API 域）
3. **生成文档**：skill 按 CK 检查点链自动执行
