# CK_VERIFY 复用验证检查点

> **源码是唯一验证标准**，禁止从函数名/状态名/注释推断行为。

## 严禁推断（CRITICAL）

- ❌ **禁止从函数名推断行为**（如 `get_nearby_prop` → "获取附近属性"，不能假设"附近 = 有效 = 可访问"）
- ❌ **禁止从状态名推断语义**（如 `HANDLE_STATE_INITED` → "已初始化 = 页表已建 = 可访问"，必须追踪状态在哪个代码路径设置、设置时目标条件是否成立）
- ❌ **禁止从头文件注释推断约束**（必须 Read 函数体追踪所有 return 路径）
- ❌ **禁止从文档"可直接用"的描述跳过验证**（文档可能不完整或语义不精确）
- ❌ **禁止挑"看起来最合理"的字段直接用，必须系统性逐字段扫描**（见下方 L5）

## 检查清单

当声明"函数 X 可直接复用"时，必须完成以下验证：

- [ ] **L1 存在性**：Grep + Read 确认函数存在于源码中，行号正确
- [ ] **L2 签名准确性**：Read 头文件确认参数类型/返回类型/个数
- [ ] **L3 返回路径追踪**：Read 函数体，穷举所有 return 路径，确认每个返回条件
- [ ] **L4 假设验证**：列出"复用此函数"的关键假设，逐个用源码验证

  > **L4 深度要求（强制）**：验证不是"确认函数能返回成功"就打勾，必须验证**返回值的精确语义对下游代码的影响**：
  >

  > 1. **读赋值表达式**：找到函数体中填充输出参数的那一行代码（如 `*end_va = ...`），读完整表达式
  > 2. **确认精确值**：不是"返回了地址"，而是"返回的是页起始地址还是末尾字节地址？是 inclusive 还是 exclusive？"
  > 3. **代入下游代码**：把精确值代入调用方的代码，验证运算结果是否正确

  >
  > **反面教训（3 次同一类型错误）**：

  > - `devmm_check_alloced_va` 返回 `*end_va` → 只验证"返回了分配范围"就打勾 → 实际是末尾字节地址 `(end+1)*page_size+start-1` → 下游用 `va = end_va` 遍历页 + `seg_size = end-start+1` → 单页时 seg_size=1 而非 page_size ❌
  > - `module_ioctl` 返回值 → 只验证"返回 int"就打勾 → 实际经 `DEVMM_KERNEL_ERR(errno_to_user_errno(errno))` 转换 → `ENOENT` 变成 `DRV_ERROR_FILE_OPS` 而非 `-ENOENT` → 下游 `ret == -ENOENT` 永远不匹配 ❌
  > - `devmm_ioctl_get_module_proc_from_file` → 只验证"走 normal path"就打勾 → 实际 `_KA_IOC_NR(cmd) < 100` 时才获取 module_proc → cmd 105/106 >= 100 → module_proc=NULL → 运行时失败 ❌

  >
  > **根因：L4 停留在"确认函数存在+签名对+有返回值"的浅层验证，没有读赋值表达式确认返回值的精确语义，没有代入下游代码验证运算结果。**

- [ ] **L5 系统性字段扫描**：当需要从现有结构体中找一个能区分两种场景的字段时，**禁止逐个试错**，必须一次性列出所有字段逐个追踪设置路径

> **L5 示例**：需要从 `module_prop` 中找能区分"本进程申请"vs"共享获得"的字段

> - 错误做法：先试 `tgid` → 再试 `priv` → 再试 `drv_mem_handle_t.type` → 最后才试 `flag`（四轮试错）
> - 正确做法：一次性列出 `module_prop` 全部 7 个字段，逐个追踪设置路径 → `flag` 能区分本地 vs import → ✅ 选定

- [ ] **L6 um_handle 前后处理检查**（新增/修改消息时强制）

  触发条件：新增 H2D/D2H/K2U 事件类型 / 修改消息结构体 / 修改消息发送路径 / 修改消息处理逻辑

  检查步骤：

  1. Grep `share_um_handle.c` 中 `svm_um_register_handle` 是否为该事件注册了 pre_handle / pre_cancel_handle / post_handle
  2. 若已注册：Read 每个 handler 函数体，记录做了什么实质性操作（如 smp_pin_mem / smp_check_mem_exists / ubmem_map_client / ksvmm_add_seg）
  3. 判断本次修改是否影响 handler 逻辑（新增事件→注册等价 handlers / 修改结构体→检查 sizeof 校验 / 修改路径→检查依赖字段 / 修改逻辑→检查 pre/post 行为）
  4. 检查 `SVM_EVENT_HANDLE_MSG_MAX_LEN`：消息变大时必须同步增大

  > **教训**：新增 `SVM_SMM_BATCH_MMAP_EVENT` 不检查 um_handle → smp_check_mem_exists / ubmem_map_client / ksvmm_add_seg 全部跳过 → 功能不正确。um_handle 对用户态透明（esched 自动调用），容易漏掉。

- [ ] **L7 跨模块调用链分析**（跨模块通信时强制）

  触发条件：新增/修改 H2D/D2H/K2U 消息 / 新增/修改 ioctl / SVM 新功能依赖外部模块

  检查步骤：

  1. 画出完整调用链：SVM 接口 → SVM 内部 → 外部模块 → 硬件/设备
  2. 逐层检查约束：消息大小限制 / 并发限制 / 缓冲区大小 / 参数校验 / 硬件限制
  3. 确认 SVM 变更不打破任何层约束
  4. **平行代码路径必须逐一验证，不能假设对称**：当一层内有平行路径（如 local/remote、send/receive、pre/post、请求/响应）时，必须逐一读每个路径的代码确认约束，不能读了 local 版就假设 remote 版一样

  > **教训 1**：只检查 SVM 内部（umc malloc 无限制），不追踪 esched → esched `sched_publish_event_para_check` 有 `msg_len > 768` 硬检查 → 消息被拒绝
>
> **教训 2**：um_handle 中 `_svm_local_submit_handle` 有 NULL handler 检查（全 NULL → 跳过 msg_len 校验），但 `_svm_remote_submit_handle` **没有**这个检查。H2D 事件在 Host 侧走 local（请求）+ remote（响应）两条路径，只读了 local 就假设 remote 一样 → 未注册 handlers 的 760B 批量消息被 remote 路径的 `msg_len > 128` 校验拦截。根因：假设了平行函数对称，没逐一验证。

- [ ] **L8 无源码时从已有证据推断**（函数实现不在本仓时强制）

  **禁止凭空假设实现**，从以下证据推断：

  1. 参数语义：函数签名中参数的来源和含义
  2. 已有代码能工作的事实：已有路径用相同参数调用且正常工作 → 反推行为
  3. 交叉引用文档：其他模块文档对该函数的描述

  确认链路：读模块 design.md → 有答案？→ 用文档结论 → 文档没覆盖？→ 读源码 → 有源码？→ 从源码确认 → 源码不在本仓？→ **告知人工处理，不假设实现**

  > **教训**：`pmq_local_pa_get` 源码不在本仓，假设查当前进程（HCCP）的 mm → 误判 register sync 不能改 udevid。实际用 `src_info->tgid`（CP1 的 tgid）查源进程 mm，pmq_design.md §4 有答案但没读。

- [ ] **L9 简化/修改数据结构时检查所有消费者**（变更数据结构格式时强制）

  列出所有消费者并逐个确认格式变更影响：

  | 消费者类型 | 检查内容 | 容易遗漏？ |
  |-----------|---------|-----------|
  | ① 发送方 | 构造消息时能否填充所有字段 | 否 |
  | ② 接收方 | 解析消息时能否还原完整信息 | 否 |
  | ③ 中间层处理者 | 是否直接依赖消息格式（如 um_handle cast 为 svm_smm_map_msg） | **是** |
  | ④ 传输层 | 是否有大小/格式校验（如 esched `msg_len > MAX`） | **是** |

  > **教训**：简化 entry 从 56B→16B，只考虑发送/接收，不考虑 um_handle 直接 cast 消息为 svm_smm_map_msg → 格式不匹配 → 运行时错误

- [ ] **L10 替换/重写函数时逐行对比副作用 + 调用链影响**（替换或重写已有函数时强制）

  **第一部分：原始函数副作用逐行对比**

  逐行列出原始函数做的所有事情，逐个确认新函数是否保留：

  | 原始函数做什么 | 类型 | 新函数是否保留？ |
  |---------------|------|----------------|
  | 过滤检查（cache/cp_only/rdonly 等） | 前置条件 | ✅/❌ |
  | 核心操作（map/unmap/collector_add 等） | 主逻辑 | ✅/❌ |
  | **per-handle 状态更新（task_bitmap 等）** | **副作用** | ✅/❌ ← **最容易丢** |
  | **per-handle 状态清理（清除位、释放引用等）** | **副作用** | ✅/❌ |
  | 错误处理 + 日志 | 错误路径 | ✅/❌ |
  | 返回值语义 | 返回值 | ✅/❌ |

  > **教训**：v1.0 的 `svm_sync_normal_addr_to_task_batch` 有 `svm_get/set_task_bitmap`（per-handle 状态更新），v2.0 重写时丢了 → `pre_free` 不 unmap 批量映射的 task_type → device 侧残留页表。根因：重写时只关注主逻辑（collector_add），没逐行对比原始函数的所有副作用。

  **第二部分：调用链上下游影响**

  原始函数被谁调用、调用谁？替换后调用链是否完整：

  | 方向 | 检查内容 | 示例 |
  |------|---------|------|
  | **上游（谁调用此函数）** | 调用方是否依赖此函数的副作用？ | `pre_free` 依赖 `task_bitmap` 被 `sync_normal` 设置过 |
  | **下游（此函数调用谁）** | 被调用方是否依赖参数变化？ | `svm_smm_client_map` 依赖 `src_info` 被 um_handle 修改后回写 |
  | **同层（ops 钩子链）** | 其他 ops 钩子是否共享状态？ | `post_malloc` 和 `sync_normal` 都写 per-handle `task_bitmap`，`pre_free` 读它 |

  > **教训**：`svm_sync_normal_addr_to_task` 被替换为 batch 版后，上游 `pre_free` 仍读 `task_bitmap`，但 batch 版不写了 → 上下游状态不一致。根因：只看函数内部逻辑，没看调用链上下游是否依赖此函数的副作用。

- [ ] **L11 数据流逐跳追踪**（编码实现时强制，L4 的编码版深化）

  > **CK_VERIFY 验证的是设计文档中的复用假设，L11 验证的是编码实现中每行新代码依赖的数据流。**
  > L4 打了勾不等于编码时可以不再追踪——编码时变量名、运算、类型转换都可能引入新问题。

  **触发条件**：每写一行新代码，只要这行代码**使用了来自其他函数的返回值/输出参数/结构体字段**，就必须执行 L11。

  **三步验证法（每行新代码）**：

  | 步骤 | 问题 | 做什么 | 示例 |
  |------|------|--------|------|
  | ① 溯源 | 这个值从哪来？ | 读产生它的那行源码，确认精确值/类型/语义 | `alloc_end_va` ← `devmm_check_alloced_va` line 478: `*end_va = (end_alloced+1)*page_size+heap->start-1` → 末尾字节地址(inclusive) |
  | ② 追跳 | 传到这里经历了什么转换？ | 逐层追踪中间每一跳（返回值→errno→宏→转换函数→数组映射） | `module_ioctl` → `ioctl()` → `errno` → `DEVMM_KERNEL_ERR` → `errno_to_user_errno` → `user_err[ENOENT]` = `DRV_ERROR_FILE_OPS`(38) |
  | ③ 代入 | 用这个值做运算/判断，结果对不对？ | 用具体数值代入，特别是边界场景（单元素、空、最大值） | 单页: `mapped_start=va`(末尾字节), `mapped_end=va`(同), `seg_size=0+1=1` ≠ `page_size` ❌ |

  **禁止的浅层验证**：

  - ❌ "函数存在+签名对+返回 0 表示成功" → 打勾
  - ❌ "返回值是 int" → 打勾
  - ❌ "返回分配范围" → 打勾

  **要求的深度验证**：

  - ✅ 读赋值表达式 → 确认精确值（页起始？末尾字节？inclusive？exclusive？）
  - ✅ 追每一跳转换 → 确认类型和值是否改变（errno→DRV_ERROR？int→DVresult？）
  - ✅ 代入具体数值 → 确认运算结果正确（单页场景？空场景？边界值？）

  > **教训**：3 个 bug 全因 L11 缺失：

  > 1. `alloc_end_va` 是末尾字节地址 → 下游当页起始用 → seg_size 算错
  > 2. `module_ioctl` 返回 `DVresult` 经 `DEVMM_KERNEL_ERR` 转换 → 下游当 `-ENOENT` 比较 → 永不匹配
  > 3. `cmd_id >= 100` 时 `module_proc=NULL` → 下游用 `module_proc->priv_data` → NULL 段错误

  >
  > **根因：编码时把 CK_VERIFY 的 ✅ 当成了"数据流已验证"，实际只验证了"函数可复用"，没有验证"返回值在下游代码中的精确行为"。**

## 文档缺陷记录

验证过程中发现子模块文档 §2 对外接口遗漏函数时，记录到缺陷清单，后续用 driver-design-md-generate 补充。
