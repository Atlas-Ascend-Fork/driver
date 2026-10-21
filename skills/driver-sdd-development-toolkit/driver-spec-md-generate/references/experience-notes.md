# API 域生成经验教训

> 从 alloc/register 域生成实践中总结的经验。供 sub-agent 参考，避免重复犯错。

## 模板演化

alloc_domain_design.md 经历 4 个版本：

- v1：初始生成，注意事项从 @attention 复制 → **错误**
- v2：L4 验证函数/返回值/调用路径 → 注意事项未重新验证
- v3：补规格矩阵 → 注意事项仍未重新验证
- v4：按 CK6_CHECK 重新验证注意事项，删除 @attention 复制内容

**教训**：注意事项必须基于源码校验逻辑，不能从头文件 @attention 复制。每次版本迭代都必须重新验证注意事项。

## 规格矩阵构建经验

| API | 参数类型 | 矩阵内容 | 源码依据 |
|-----|---------|---------|---------|
| halMemAlloc | flag (12 bit 域) | 位域布局+8值+15约束+12组合+capability | svm_parse_alloc_devid + svm_parse_alloc_svm_flag |
| halMemCreate | struct prop | 字段规格+side+devid组合+pg_type约束+对齐表 | vmm_prop_check + vmm_get_create_granularity |
| halMemPoolCreate | struct prop | 10字段约束+合法/非法组合 | soma_pool_create_para_check |
| halMemPoolSetAttr | enum+value | 9种组合+可写属性汇总 | soma_pool_check_attr_val |
| halMemPoolAsyncConfig | flag+va+size | flag取值+va/size约束 | halMemPoolAsyncConfig + soma_validate_cfg_params |
| halMemAddressReserve | flag+addr | 位域布局+7约束+自动调整+对齐表 | vmm_address_reserve_para_check |

**关键**：规格矩阵的源码依据是**参数解析/校验函数**，不是 API 入口函数本身。

## L4 验证经验（头文件注释不准确实例）

| 验证项 | 头文件注释 | 源码实际 | 差异 |
|--------|----------|---------|------|
| halMemFree 返回值 | `@return DV_ERROR_XXX` | BUSY→返回 0，CLIENT_BUSY→返回 BUSY | 头文件太笼统 |
| halMemAlloc flag 约束 | `@param flag: requested memory parameter` | svm_parse_alloc_svm_flag 有 15 条约束 | 头文件未提及 |
| halMemPoolCreate 约束 | `@param prop` | soma_pool_create_para_check 有 6 项硬约束 | 头文件未提及 |
| halMemPoolSetAttr | `@param attr` | soma_pool_check_attr_val 有 5 种组合 | 头文件未提及 |
| halMemAddressReserve | `@attention "仅支持指定地址预留"` | V3 源码未校验此约束（V2 有校验，V3 遗漏） | 头文件与 V3 实现不符 |
| halMemAlloc size | `@attention "最大 128GB"` | api_master 层无上限校验 | 头文件声称但源码未实现 |
| halMemAdvise | `@brief "advise memory"` | 只支持 Device 内存，不支持 Host | 头文件过于笼统 |
| halMemcpy | `@attention "Only support d2h"` | info=NULL 时支持通用拷贝 | 头文件不准确 |

**教训**：头文件注释在以下方面经常不准确或遗漏：

1. 参数组合约束（哪些 flag 组合非法）
2. 返回值的触发条件
3. 枚举参数的合法值范围
4. 只读/只写属性约束
5. 对齐要求
6. V2→V3 重写时遗漏的校验逻辑

## 常见错误与规避

| 错误 | 规避方法 |
|------|---------|
| 从 @brief 推断功能 | 必须 Read 函数体确认实际逻辑 |
| 从 @param 推断约束 | 必须 Read 校验函数的所有 if/switch 分支 |
| 从 @return 推断返回值 | 必须追踪所有 return 路径 |
| 从 @attention 复制注意事项 | 必须基于源码校验逻辑，标注 file:line |
| 只记录成功路径 | 每个错误返回路径都是规格矩阵一行 |
| 合并相似条件 | 每个 if 分支独立编号 |
| 遗漏 else/default | 最后的 return 0 也要记录 |
| 从函数名推断参数语义 | 必须 Read 调用者代码确认 |
| 从单一枚举值泛化 | 每个枚举值独立 Read 校验分支 |

## register 域经验教训

### 函数功能描述过于简略

halHostRegister 功能描述写的是"将 Host 内存注册到 Device 侧"，但源码 `switch(map_type)` 明确有 `DEV_SVM_MAP_HOST`（Device→Host）方向。虽然不是从头文件复制，但**功能描述没有反映双向注册特性**。

**教训**：函数功能须基于源码 `switch`/`if` 分支分析，体现 API 的核心行为（如双向注册、路径分发、自动转换等），不能过于简略。

### 缺少地址来源规格

register 域文档有 map_type 取值矩阵，但没有描述每个 map_type 的 `src_ptr` 实际来源（`malloc`/`halMemAlloc(MEM_DEV)`/`mmap` 等）。用户需要知道"这个参数传什么类型的地址"。

**教训**：每个输入参数须描述实际来源和要求，作为规格文档的必备内容。

### 缺少前置条件与依赖

register 域文档有注意事项和调用路径，但没有描述前置条件（如须先 `halMemAgentOpen`、须先 `halMemAlloc` 分配、须支持某 capability）。注意事项是"使用约束"（如不支持大页），前置条件是"调用前必须满足的条件"（如须先初始化），两者不同。

**教训**：每个 API 须描述调用前必须满足的条件和依赖的接口，作为规格文档的必备内容。

### 地址来源规格机械添加

skill 新增"地址来源规格"后，机械地为所有 56 个 API 都添加了，包括 devid/flag/type/handle/shareable_handle 等非地址参数的 API。用户指出"有实际地址的需要加，没有地址的这个章节可以省略，不要机械的加上"。

**教训**：地址来源规格仅对有实际地址参数（ptr/src_ptr/va/pp 等）的 API 需要。非地址参数的 API 省略此节。skill 规则已更新为"仅有实际地址参数的 API 才需要"。

### copy 域地址来源表述不准确

halMemcpy 的 dst/src 来源写成"halMemAlloc 分配的 SVM VA"，但实际 dst/src 可能来自 `malloc`/`mmap`（Host 普通内存）、共享池 VA 或 SVM VA。用户指出"copy的源和目的的地址不是一定来源于halMemAlloc的，表述存在严重问题"。

**教训**：地址来源须基于源码完整路径分析，不能只看 capability 校验路径就推断来源。`svm_set_copy_va_info` 内部可能处理非 SVM VA（Host 普通内存）的路径，须 Read 到实现函数确认。

### 根本原因

以上三个问题的共同根因：skill 规则不精确 + 执行时没思考。skill 说"每个输入参数须描述实际来源"但没限定"仅地址参数"；执行时没有判断"这个参数是否真的是地址"就机械添加。skill 已更新为"仅有实际地址参数的 API 才需要"。

这三项内容在 skill 的 CK6_CHECK 完整性清单和 api-master-template.md 中**没有定义为必备规格项**。不是执行不到位，而是**规则缺失**。skill 已更新为 10 项必备规格。

## vmm_handle 域经验教训

### 调用路径缺失

vmm_handle 域 17 个 API **全部缺失调用路径**。根因：sub-agent 超时后由主 agent 快速生成时，为了节省 context 省略了调用路径、返回值说明、支持场景、注意事项等规格块。后续补做了地址来源和前置条件，但调用路径仍然缺失。

**教训**：

1. 快速生成时不能省略 10 项必备规格中的任何一项——即使 context 紧张，也要在后续轮次补齐
2. 保留旧版本或快速生成后**必须走 CK6_CHECK**，逐项核对 10 项必备规格是否完整
3. skill 中"调用路径"已经是 10 项必备规格之一，规则没有缺失——问题出在执行时没检查

### 根本原因

vmm_handle 域的问题与 alloc/register 域不同：

- alloc/register 域：skill **规则缺失**（没定义地址来源/前置条件为必备项）
- vmm_handle 域：skill **规则已有**（调用路径是必备项），但**执行时没走 CK6_CHECK**
- ipc 域：experience-notes 已写"必须走 CK6_CHECK"，**但生成时仍没执行**——4 个 API 缺失输入说明

两类问题的解决方式不同：

- 规则缺失 → 更新 skill 规则
- 执行不到位 → 保留旧版本时必须走 CK6_CHECK 完整性清单逐项核对

## uvm 域经验教训

### 未实现 API 加了过多说明

uvm 域 4 个未实现 API（halMemManagedAdvise/halMemManagedRangeGetAttributes/halCheckProcessStatus/halCheckProcessStatusEx）包含大量 v2 参考行为描述、约束矩阵、返回值说明、调用路径等。用户要求"直接标记未实现即可，不要加其他说明"。

**教训**：未实现的 API 只保留函数原型 + "未实现"标记。不加 v2 参考行为、约束矩阵、返回值说明、调用路径、注意事项等任何内容。skill 已更新规则和模板。

### 规则写了教训记了但仍然不执行

ipc 域生成时，experience-notes.md 中已经写了"必须走 CK6_CHECK"的教训（来自 vmm_handle 域）。但生成 ipc 域后**仍然没有执行自检**——4 个 API（halShmemSetAttribute/halShmemGetAttribute/halShmemOpenHandleByDevId/halShmemOpenHandle）缺失输入说明。

**根因**：CK6_CHECK 是"文档生成后统一检查"，但生成时进入"写内容"的心流，没有停下来检查。经验写在 experience-notes.md 中，但生成时没有去读。

**教训**：

1. **不能依赖记忆执行检查**——必须在流程结构中强制检查
2. **CK6_CHECK 从"文档生成后统一检查"改为"逐 API 生成后立即自检"**——每个 API 生成后对照 10 项清单逐项检查，不进入下一个 API
3. skill 已更新：CK4 拆分为"逐 API 生成 → 自检 → 下一个 API"，模板中嵌入自检清单

### 自检清单残留在最终文档中

Sisyphus-Junior 严格按模板执行，将 CK4.5 自检清单（HTML 注释 + markdown 清单）保留在了所有 8 个域文档中（共 77 处）。这些是生成过程中的检查工具，不应该出现在交付文档中。

**根因**：模板中嵌入自检清单时，没有明确标注"确认后删除"。sub-agent 按模板照搬，保留了自检块。

**教训**：模板中的自检清单必须明确标注为"临时检查工具，确认后删除"。skill 已更新：

- 模板中自检块用注释标注"确认 10 项完整后删除本块"
- CK6_CHECK 增加"0 CK4.5 自检标记"检查项
- CK4 禁止项增加"将自检清单保留在最终文档中"

### 根本原因：执行纪律 vs 结构化保障

| 方式 | 依赖 | 可靠性 |
|------|------|--------|
| "记得要检查" | 记忆 | ❌ 不可靠——生成时进入心流会忘 |
| "流程结构强制检查" | 模板+清单 | ✅ 可靠——每个 API 生成后必须对照清单 |

skill 已从"依赖记忆"改为"依赖结构化流程"：

- CK4 生成流程改为"逐 API 生成 → 自检 → 下一个 API"
- 模板中每个 API 规格块末尾嵌入 10 项自检清单
- 禁止"当前 API 10 项未完整前生成下一个 API"
