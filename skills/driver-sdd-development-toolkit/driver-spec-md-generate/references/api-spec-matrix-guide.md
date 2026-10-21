# API 规格矩阵构建方法论

> 本文件详细说明如何从源码构建 API 规格矩阵——这是 driver-spec-md-generate skill 的核心产出。
> 以 alloc 域 6 个复杂 API 为实例，展示矩阵构建的完整流程。

---

## 核心理念

**规格矩阵回答的问题**：给定一个 API，用户传哪些参数组合是合法的？哪些是非法的？非法时返回什么错误码？

**信息来源**：不是头文件 `@param` 注释，不是 README，而是源码中**参数解析/校验函数的所有 return 路径**。

---

## 构建流程（5 步）

### 步骤1：定位参数解析/校验函数

API 入口函数（如 `halMemAlloc`）通常只做薄包装，真正的参数校验在内部函数中。

**方法**：

1. Grep API 函数名 → Read 实现函数体
2. 识别函数体中的校验调用（函数名通常含 `parse`/`check`/`validate`/`para_check`）
3. Read 这些校验函数的**完整函数体**

**实例**（halMemAlloc）：

```text

halMemAlloc (svm_alloc.c:327)
  └─ halMemAllocInner (svm_alloc.c:292)
       └─ svm_mem_malloc (svm_alloc.c:211)
            ├─ svm_parse_alloc_devid (svm_alloc.c:101)   ← flag 解析函数1
            ├─ svm_parse_alloc_module_id (svm_alloc.c:123) ← flag 解析函数2
            └─ svm_parse_alloc_svm_flag (svm_alloc.c:132)  ← flag 校验函数（核心）

```

校验函数定位后，Read 其完整函数体，准备穷举所有分支。

### 步骤2：穷举所有 return 路径

对校验函数中的**每个 if/switch 分支**，记录 4 项信息：

| 信息 | 说明 | 示例 |
|------|------|------|
| **条件** | 触发该 return 的条件 | `flag & MEM_ADVISE_4G` |
| **返回码** | 该分支的返回值 | `DRV_ERROR_NOT_SUPPORT` |
| **源码行** | 该 return 语句的行号 | `:137` |
| **编号** | 矩阵行号 | `#1` |

**实例**（`svm_parse_alloc_svm_flag` svm_alloc.c:132-209 的 15 条约束）：

```c

// :137 — 约束 #1
if ((flag & MEM_ADVISE_4G) || (flag & MEM_ADVISE_TS)) {
    return DRV_ERROR_NOT_SUPPORT;
}

// :141 — 约束 #3
if (flag & MEM_READONLY) {
    return DRV_ERROR_NOT_SUPPORT;
}

// :145-148 — 约束 #4
if (((virt_mem_type == MEM_HOST_VAL) || (virt_mem_type == MEM_HOST_UVA_VAL)) &&
    ((flag & MEM_PAGE_HUGE) || (flag & MEM_PAGE_GIANT))) {
    return DRV_ERROR_NOT_SUPPORT;
}

// ... 逐个分支记录

```

**关键原则**：

- **禁止只记录成功路径**——每个错误返回路径都是规格矩阵的一行
- **禁止合并相似条件**——每个 `if` 分支独立编号
- **禁止遗漏 else/default**——最后的 `return 0`（合法）也要记录

### 步骤3：构建约束矩阵表

将步骤2 的穷举结果整理为表格：

```markdown

**flag 组合约束矩阵**（源码: `svm_parse_alloc_svm_flag` svm_alloc.c:132-209）:

| # | 约束条件 | 返回码 | 源码行 |
|---|---------|--------|--------|
| 1 | `MEM_ADVISE_4G` 置位 | `NOT_SUPPORT` | :137 |
| 2 | `MEM_ADVISE_TS` 置位 | `NOT_SUPPORT` | :137 |
| 3 | `MEM_READONLY` 置位 | `NOT_SUPPORT` | :141 |
| 4 | (`HOST` 或 `HOST_UVA`) 且 (`HUGE` 或 `GIANT`) | `NOT_SUPPORT` | :145-148 |
| 5 | `HOST_UVA` 且 `CONTIGUOUS_PHY` | `NOT_SUPPORT` | :150-153 |
| ... | | | |
| — | 无以上任何冲突 | **合法** | — |

```

**格式要求**：

- 表头固定：`# | 约束条件 | 返回码 | 源码行`
- 最后一行用 `—` 编号表示"无冲突=合法"
- 标注源码函数名和行号范围

### 步骤4：构建合法组合示例表

从约束矩阵中推导典型组合，选取**有代表性的组合**（不需要穷举所有合法组合）：

```markdown

**合法 flag 组合示例**:

| virt_mem_type | 页大小 | CONTIGUOUS | P2P/BAR | DEV_CP_ONLY | HOST_RW_DEV_RO | 合法 |
|---------------|--------|-----------|--------|-------------|----------------|------|
| DEV (0x1) | NORMAL | ✗ | ✗ | ✗ | ✗ | ✓ |
| DEV (0x1) | HUGE | ✗ | ✗ | ✗ | ✗ | ✓ |
| DEV (0x1) | GIANT | ✗ | ✗ | ✗ | ✗ | ✓ |
| HOST (0x2) | HUGE | ✗ | ✗ | ✗ | ✗ | ✗ (约束#4) |
| ... | | | | | | |

```

**选取原则**：

- 覆盖主要枚举值（如 DEV/HOST/HOST_UVA/DVPP）
- 覆盖关键组合维度（页大小/连续物理/P2P/特殊标志）
- 包含至少 1 个非法组合示例（标注违反哪条约束）

### 步骤5：构建参数特定子表

根据参数类型，额外构建以下子表：

#### flag 参数 → 位域布局表

```markdown

**flag 位域布局**（源码: `ascend_hal_define.h:717-781`）:

| 位域 | 位 | 宏 | 说明 |
|------|-----|------|------|
| devid | 0-9 | `MEM_DEVID_MASK` (10 bits) | 设备 ID |
| virt_mem_type | 10-13 | `<< MEM_VIRT_BIT` (4 bits) | 内存类型 |
| page_huge | 17 | `MEM_PAGE_HUGE` | 大页 (2MB) |
| ... | | | |

```

**构建方法**：

1. Grep flag 位域宏定义（如 `#define MEM_PAGE_HUGE`）
2. 记录每个位域的位号、宏名、说明
3. 标注源码头文件和行号

#### struct 参数 → 字段规格表

```markdown

**prop 字段规格**（源码: `vmm_prop_check` svm_vmm.c:325-385）:

| 字段 | 类型 | 合法取值 | 约束 | 返回码 | 源码行 |
|------|------|---------|------|--------|--------|
| `side` | `enum` | `MEM_HOST_SIDE`(0)/`MEM_DEV_SIDE`(1)/`MEM_HOST_NUMA_SIDE`(2) | < `MEM_MAX_SIDE`(3) | `INVALID_VALUE` if >= MAX | :332 |
| `devid` | `uint32_t` | 依 side 而定 | 见 side+devid 组合表 | | |
| ... | | | | | |

```

**构建方法**：

1. Read 结构体定义 → 记录字段名、类型
2. Read 校验函数 → 记录每个字段的约束和返回码
3. 枚举类型 → Grep 枚举定义，列出所有合法值

#### enum 参数 → 取值规格表

```markdown

**attr + value 组合规格**（源码: `soma_pool_check_attr_val` svm_soma.c:600-648）:

| attr 值 | 宏 | value (`int64_t`) | 约束 | 返回码 | 源码行 |
|---------|-----|-------------------|------|--------|--------|
| 0x1 | `RELEASE_THRESHOLD` | ≥0 且 2MB 对齐 | 合法 | `NONE` | :604-613 |
| 0x1 | `RELEASE_THRESHOLD` | < 0 | 负数 | `INVALID_VALUE` | :604-608 |
| ... | | | | | |

```

**构建方法**：

1. Grep 枚举定义 → 列出所有枚举值和宏名
2. Read switch/if 分支 → 记录每个枚举值的约束
3. 对每个枚举值，记录合法/非法 value 范围

#### 额外子表

根据 API 特性，可能还需要：

| 子表类型 | 适用场景 | 实例 |
|---------|---------|------|
| **capability 自动设置表** | API 内部根据入参自动设置内部 flag | `svm_mem_malloc` 根据 virt_mem_type 设置 REGISTER/COPY/MEMSET 等 |
| **对齐粒度表** | size 参数的对齐要求随其他参数变化 | `vmm_get_create_granularity` 根据 pg_type/module_id/size 返回不同粒度 |
| **side+devid 组合表** | struct 参数中两个字段有联合约束 | `vmm_prop_check` 中 side 和 devid 的联合校验 |
| **自动调整逻辑表** | API 内部对入参做自动修正（非校验） | `halMemAddressReserve` 中 size>=1GB 自动升级为 GIANT_PAGE_TYPE |

---

## 6 种矩阵类型速查

| 矩阵类型 | 适用参数 | 构建依据 | 实例 API |
|---------|---------|---------|---------|
| **位域布局表** | 复合 flag 参数 | Grep 位域宏定义 | halMemAlloc(flag) |
| **约束矩阵** | 所有复杂参数 | 校验函数所有 if/switch 分支 | halMemAlloc/halMemCreate/halMemPoolCreate |
| **合法组合示例表** | 多维度组合参数 | 从约束矩阵推导 | halMemAlloc/halMemPoolCreate |
| **字段规格表** | struct 参数 | 结构体定义 + 校验函数 | halMemCreate(prop) |
| **取值规格表** | enum 参数 | 枚举定义 + switch 分支 | halMemPoolSetAttr(attr) |
| **自动设置/调整表** | API 内部自动行为 | 函数体中的 flag 设置逻辑 | halMemAlloc(capability)/halMemAddressReserve(自动升级) |

---

## 哪些 API 不需要规格矩阵

| 参数类型 | 是否需要 | 原因 | 处理方式 |
|---------|---------|------|---------|
| 简单指针（如 `void *ptr`） | ❌ | 无组合约束 | 参数表 + 源码验证注释 |
| 保留参数（必须为 0） | ❌ | 无组合 | 参数表注明"必须为 0" |
| 简单整数（如 `size`） | ⚠️ 视情况 | 如有对齐要求则需对齐表 | 有约束则建表，无则注释 |
| 复合 flag | ✅ | 多 bit 域组合 | 位域布局 + 约束矩阵 + 合法组合 |
| struct | ✅ | 多字段联合约束 | 字段规格 + 组合约束 |
| enum | ✅ | 枚举值 + 值组合 | 取值规格 + value 组合 |

**判断标准**：参数是否有"组合约束"（即参数的某个维度影响另一个维度的合法性）。有→需要矩阵；无→不需要。

---

## 实例：alloc 域 6 个 API 的矩阵构建

### halMemAlloc — flag 参数（最复杂）

**源码依据**：`svm_parse_alloc_devid` (svm_alloc.c:101-121) + `svm_parse_alloc_svm_flag` (svm_alloc.c:132-209)

**构建的矩阵**：

1. ✅ flag 位域布局表（12 位域）
2. ✅ virt_mem_type 取值规格表（8 个值 × 约束 × 返回码）
3. ✅ flag 组合约束矩阵（15 条约束）
4. ✅ 合法组合示例表（12 行典型组合）
5. ✅ capability 自动设置表（3 种场景）

### halMemCreate — struct prop 参数

**源码依据**：`vmm_prop_check` (svm_vmm.c:325-385) + `vmm_get_create_granularity` (svm_vmm.c:1010-1019)

**构建的矩阵**：

1. ✅ prop 字段规格表（6 字段 × 约束 × 返回码）
2. ✅ side+devid 组合约束表（4 种 side × devid 约束）
3. ✅ pg_type+side+mem_type 组合约束表（GIANT 的 2 条禁止约束）
4. ✅ size 对齐粒度表（pg_type × module_id × size → 粒度）
5. ✅ Device 侧连续物理约束表（自动行为）

### halMemPoolCreate — struct prop 参数

**源码依据**：`soma_pool_create_para_check` (svm_soma.c:477-528)

**构建的矩阵**：

1. ✅ prop 字段规格表（10 字段 × 约束 × 返回码 × 源码行）
2. ✅ 合法/非法组合示例表（10 行）

### halMemPoolSetAttr — enum+value 参数

**源码依据**：`soma_pool_check_attr_val` (svm_soma.c:600-648)

**构建的矩阵**：

1. ✅ attr+value 组合规格表（9 种组合 × 约束 × 返回码）
2. ✅ 可写属性汇总表（5 种 attr × 可写/只读 × 合法 value）

### halMemPoolAsyncConfig — flag+va+size 参数

**源码依据**：`halMemPoolAsyncConfig` (svm_soma.c:954-975) + `soma_validate_cfg_params` (svm_soma.c:824-844)

**构建的矩阵**：

1. ✅ flag 取值规格表（2 合法值 + 其他→NOT_SUPPORT）
2. ✅ va+size 组合约束表（6 条约束）
3. ✅ 合法 va/size 范围说明

### halMemAddressReserve — flag+addr 参数

**源码依据**：`vmm_address_reserve_para_check` (svm_vmm.c:780-822) + `halMemAddressReserve` (svm_vmm.c:833-869)

**构建的矩阵**：

1. ✅ flag 位域布局表
2. ✅ 参数约束矩阵（7 条约束）
3. ✅ 自动调整逻辑表（2 种自动行为）
4. ✅ pg_type 对齐要求表（3 种 pg_type → 粒度）

---

## 常见错误与规避

| 错误 | 示例 | 规避方法 |
|------|------|---------|
| **从头文件注释推断约束** | 从 `@param flag: requested memory parameter` 推断 flag 约束 | 必须 Read `svm_parse_alloc_svm_flag` 源码 |
| **只记录成功路径** | 返回值表只列 `DRV_ERROR_NONE` + `DV_ERROR_XXX` | 追踪所有 return 路径，逐个列出错误码 |
| **合并相似条件** | 将 5 条 HOST_UVA 约束合并为 1 行 | 每个 `if` 分支独立编号 |
| **遗漏 else/default** | 校验函数最后的 `return 0` 未记录 | 最后一行用 `—` 编号表示"合法" |
| **从函数名推断参数语义** | 从 `svm_parse_alloc_devid` 推断 devid 含义 | Read 调用者代码确认参数实际传入值 |
| **从单一枚举值泛化** | 读了 DEV 的约束就推广到所有 virt_mem_type | 每个枚举值独立 Read 校验分支 |
| **不标注源码行号** | 约束矩阵无 `:line` 列 | 每条约束标注 `源码行` 列 |
