# API 规格文档模板规范

> 本文件定义 **api_master 层**的文档模板：per-API `{API}_spec.md`（必须）+ 可选域级 `{domain}_design.md`。

---

## 1. per-API spec 模板（每个 API 一个文件）

文件名：`{API}_spec.md`，放在 `1_support/api_master/api/{domain}/` 目录下。

```markdown

# {API名} 接口规格

> 所属域: {domain} | 域索引: [spec.md](../../../spec.md) §2 | 实现: {hal_src_dir}/api/master/{file}:{line}

---

#### {API名}

**函数原型**:

```c

// 声明: pkg_inc/ascend_hal_base.h:{L}
// 实现: {hal_src_dir}/api/master/{file}:{L}
{返回类型} {API名}({参数列表});

```text

**函数功能**: {基于源码 switch/if 分支分析，体现核心行为}

**输入说明**:

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| {param} | {type} | [in/out] | {说明} |

{如有复杂参数（flag/struct/enum），追加规格矩阵/位域布局/取值表}

**地址来源规格**: {仅有地址参数的 API 需要，非地址参数 API 省略}

| 参数/路径 | 来源 | 要求 | 源码依据 |
|-----------|------|------|---------|
| {addr_param} | {来源} | {要求} | {file:line} |

**输出说明**:

| 参数 | 类型 | 方向 | 说明 |
|------|------|------|------|
| {param} | {type} | [out] | {说明} |

**返回值说明**:

| 返回值/错误码 | 说明 | 处理建议 |
|--------------|------|----------|
| `DRV_ERROR_NONE` (0) | {成功说明} | — |
| `DRV_ERROR_XXX` | {失败说明} | {处理建议} |

> 穷举所有 return 路径，禁止只列成功+"其他错误"

**前置条件与依赖**:

| 前置条件 | 依据 | 说明 |
|----------|------|------|
| {须先调的接口} | {源码依据} | {说明} |

**支持场景**: {基于源码 feature switch，列出支持的芯片/部署模式}

**注意事项**: {基于源码校验逻辑，禁止从 @attention 复制}

**调用路径**:

```

{API名} ({file}:{L})
  ├─ {函数1} ({file}:{L})
  ├─ {函数2} ({file}:{L})
  └─ → 详见 [{子模块}_design.md]

```text

```

### 未实现 API 模板

```markdown

# {API名} 接口规格

> 所属域: {domain} | 域索引: [spec.md](../../../spec.md) §2 | 实现: 无

---

#### {API名}

**函数原型**:

```c

// 声明: pkg_inc/ascend_hal_base.h:{L}
DLLEXPORT ... {API名}(...);

```text

**未实现**：当前路径无实现（`{原因}`）。

```

---

## 2. 顶层 spec.md 模板（域索引）

```markdown

# 接口规格总览（spec.md）

> **文档定位**: 顶层索引/概览，汇总所有 HAL 公共 API，按功能域分组。
> 每个接口的详细规格见对应域目录下的 `{API名}_spec.md` 文件。

---

## 1. 模块概述

{2-3 句话概述模块核心定位}

---

## 2. 接口架构

### 8 个功能域

| # | 功能域 | 接口规格目录 | API 数 | 职责 |
|---|--------|-------------|--------|------|
| 1 | **alloc** | [`alloc_domain/`](1_support/api_master/api/alloc_domain) | {N} | {职责} |
| 2 | **op** | [`op_domain/`](1_support/api_master/api/op_domain) | {N} | {职责} |
| ... | ... | ... | ... | ... |

> **合计**: {N} 个 HAL 公共 API。

---

## 3. 变更记录

| 版本 | 日期 | 变更内容 | 作者 |
|------|------|---------|------|
| v1.0 | {date} | {说明} | {author} |

```

---

## 3. 可选域级文档模板（{domain}_design.md）

> **非必须**。当域内有多个 API 需要描述组合使用模式时生成。放在 `1_support/api_master/` 目录下。

```markdown

# {功能域名} 设计文档

> **分类**: HAL 接口实现层 · {功能域名} | **位置**: `{hal_src_dir}/api/master/` | **API 数**: {N}

---

## 1. 软件功能

{2-3 句话概述本功能域核心职责}

**实现文件**:

| 文件 | 功能 |
|------|------|
| `{hal_src_dir}/api/master/{file}` | {功能} |

**依赖子模块**:

| 子模块 | 提供能力 |
|--------|---------|
| `{子模块}` | {能力} |

---

## 2. HAL 接口总览

| API | 功能 | 详细规格 |
|-----|------|---------|
| `{API}` | {功能} | [{API}_spec.md](api/{domain}/{API}_spec.md) |

---

## 3. 关键设计要点

{域级设计决策、路由逻辑、feature switch 策略等}

---

## 4. 典型组合使用模式

{至少 1 个 Mermaid 时序图，展示域内 API 组合使用}

```mermaid

sequenceDiagram
    participant App as 应用
    participant API as HAL API
    participant Sub as 子模块

    App->>API: {API1}(...)
    API->>Sub: {函数}(...)
    Sub-->>API: ret
    App->>API: {API2}(...)
    API->>Sub: {function}(...)

```text

---

## 5. 文件索引

| 文件 | 功能 |
|------|------|
| `{hal_src_dir}/api/master/{file}` | {功能} |

```

---

## 与子模块模板的关系

| 模板 | 适用对象 | 粒度 | 核心产出 |
|------|---------|------|---------|
| `design_template.md`（子模块 skill） | 内部子模块 | 单子模块内部实现 | 架构图 + 结构体 + 算法逻辑 |
| **本模板** | HAL 接口实现层 | per-API 规格 + 可选域级概述 | **规格矩阵**（约束 + 合法组合） |
