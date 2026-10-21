# CK_CODE 代码实现检查点

> **代码实现阶段同样必须执行文档约束检查。**

## CK_CODE_GATE: 写代码前必须确认的约束（强制）

在写任何新 .c/.h 文件或修改现有文件之前，逐项确认：

- [ ] **注释语言**：查 AGENTS.md §3.5 → 必须英文注释（❌ 禁止中文）
- [ ] **对外头文件类型**：查 AGENTS.md §3.5 → 地址用 `unsigned long long`、devid 用 `unsigned int`、bool 用 `bool`（❌ 禁止 `u64`/`u32`）
- [ ] **include 顺序**：查 AGENTS.md §3.5 → 系统 `<...>` → 空行 → `ascend_hal.h` → 依赖头文件 → **自己的 .h 放最后**
- [ ] **日志约束**：查 AGENTS.md §3.2 → 模块日志宏（如 `{module}_warn`/`{module}_info`/`{module}_debug`）中 ❌ 禁止出现 `fail`/`err`/`error` 字眼
- [ ] **错误处理**：查 AGENTS.md §3.1 → 必须返回 `DRV_ERROR_xxx` + 检查返回值 + 错误分支打日志
- [ ] **资源清理**：查 AGENTS.md §3.3 → 错误分支反向释放已分配资源

> **经验教训（4 次同一类型错误）**：

> - include 顺序 → 假设"自己 .h 放前面" → 实际应放最后
> - 中文注释 → 用中文注释 → 实际必须英文
> - u64/u32 类型 → 用内部类型 → 实际应 `unsigned long long`/`unsigned int`
> - 日志含 "failed" → 模块日志宏中用了 "failed" → 实际禁止

>
> **根因：写代码时凭经验/习惯假设，没有回查文档约束。**

## CK_CODE_DATAFLOW: 编码时数据流逐行验证（强制）

> **CK_VERIFY 的 L4 验证的是设计文档中的复用假设；CK_CODE_DATAFLOW 验证的是编码实现中每行新代码依赖的数据流。L4 打了勾不等于编码时可以不再追踪——编码时变量名、运算、类型转换都可能引入新问题。**

**核心原则：每写一行新代码，只要使用了来自其他函数的返回值/输出参数/结构体字段，就必须执行数据流三步验证。**

### 三步验证法

每行新代码，回答三个问题：

| 步骤 | 问题 | 做什么 | 不做的后果 |
|------|------|--------|-----------|
| ① 溯源 | 这个值从哪来？ | Read 产生它的那行源码，确认精确值/类型/语义 | 拿到末尾字节地址当页起始用 |
| ② 追跳 | 传到这里经历了什么转换？ | 逐层追踪中间每一跳（返回值→errno→宏→转换函数→数组映射） | errno 经转换变成 DRV_ERROR，下游当原值比较 |
| ③ 代入 | 用这个值做运算/判断，结果对不对？ | 用具体数值代入，特别是边界场景（单元素、空、最大值） | 单页时 size=1 而非 page_size |

### 禁止的浅层验证

- ❌ "函数存在+签名对+返回 0 表示成功" → 打勾
- ❌ "返回值是 int" → 打勾
- ❌ "返回分配范围" → 打勾

### 要求的深度验证

- ✅ 读赋值表达式 → 确认精确值（页起始？末尾字节？inclusive？exclusive？）
- ✅ 追每一跳转换 → 确认类型和值是否改变（errno→DRV_ERROR？int→DVresult？）
- ✅ 代入具体数值 → 确认运算结果正确（单页场景？空场景？边界值？）

### 经验教训（3 次同一类型错误）

| # | 代码 | 浅层验证（❌ 打了勾就过） | 深度验证（✅ 应该做的） | 后果 |
|---|------|------------------------|------------------------|------|
| 1 | `va = alloc_end_va` | "返回分配范围 ✅" | 读 line 478: `(end+1)*page_size+start-1` → 末尾字节地址 → 代入 `seg_size=end-start+1` → 单页时=1 | seg_size 算错 |
| 2 | `ret == -ENOENT` | "返回 int ✅" | 追 `DEVMM_KERNEL_ERR`→`errno_to_user_errno`→`user_err[ENOENT]`=`DRV_ERROR_FILE_OPS`(38) | 永不匹配 |
| 3 | `cmd_id = 105` | "走 normal path ✅" | 读 line 399: `_KA_IOC_NR(cmd) < 100` 才获取 module_proc → 105 >= 100 → module_proc=NULL | 运行时失败 |

> **根因：把 CK_VERIFY 的 ✅ 当成"数据流已验证"，实际只验证了"函数可复用"，没有验证"返回值在下游代码中的精确行为"。编码不是抄伪码——每行代码依赖的数据都要从源头逐跳追到使用点，确认精确值和语义。**

## CK_CODE_COMPILE: 编译检查（强制）

> **编码完成、CK_CODE_REVIEW 之后、提交 PR 之前，必须执行编译检查。** 编码规范检查不能替代编译验证——类型不匹配、缺少 include、符号未定义等问题只有编译器能发现。

执行步骤：

1. 确认编译方式：查 `build.sh` / `CMakeLists.txt` / `Makefile` 确定编译命令
2. 执行编译：`bash build.sh` 或 `cmake --build build/` 或 `make`
3. 检查结果：编译必须 0 error，0 warning（如有历史 warning 需确认不是本次引入的）

> **常见编译错误**：

> - 缺少 include（如 `securec.h` for `memcpy_s`）
> - 类型不匹配（如 `u32` 传给 `unsigned int` 参数的函数，在对外头文件中类型不同）
> - 未定义符号（如新增函数声明在 .h 但实现在 .c 中忘了写）
> - 结构体大小变化导致 `sizeof` 校验失败（如 um_handle 中 `msg_len != sizeof(*map_msg)`）

## CK_CODE_REVIEW: 代码提交前 review sub-agent（强制）

> **自己检查等于不检查。** 必须分派 review sub-agent 专做检查。

编码完成后、提交 PR 前，分派 review sub-agent：

```text

task(
  category="unspecified-high",
  run_in_background=false,
  description="Code review against coding standards",
  prompt="""
  ## TASK
  Review the code changes against coding standards. Output pass/fail for each item.

  ## STEP 0: LOAD REVIEW RULES (先读规则再读代码)

  1. Read AGENTS.md §3.1-§3.5 + §4
  2. Read `gitcode-pr-review/references/driver_coding_standards.md` if exists
  3. Read `gitcode-pr-review/references/outparam_rollback_rules.md` if exists
  4. Read `gitcode-pr-review/references/global_instance_concurrency_rules.md` if exists
   5. Load module-specific rules as needed:
      - Module-specific changes → module-specific rules file (if exists)
     - Kernel MMIO → ioremap_iomem_rules.md
     - PCIE interface → pcie_general_rules.md
     - Spinlock usage → spinlock_audit_rules.md
     - Sync event → sync_event_timeout_engine_rules.md

  ## FILES TO REVIEW
  {列出所有新增/修改的文件路径}

  ## REVIEW SCOPE
  ### From AGENTS.md §3.5 (code style):

  - Comments in English? (grep non-ASCII)
  - Public header types: no u64/u32/u8 in pkg_inc/*.h?
  - Include order: system → ascend_hal.h → deps → own .h last?

  ### From AGENTS.md §3.1-§3.2 (error handling + log):

  - Return DRV_ERROR_xxx for public APIs?
  - All function return values checked?
   - No "fail"/"err"/"error" in module log macros (warn/info/debug)?
   - Use ratelimited logging for high-frequency logs?

  ### From AGENTS.md §3.3 (resource cleanup):

  - Error branches release allocated resources?

  ### From driver_coding_standards.md (general):

  - Pointer params checked for NULL?
  - Array params have length param?
  - No unused includes? Header #define guard present?
  - switch has default branch? No realloc()?
  - Format string: %u/%d/%llx type matches argument type?
  - Format string: argument count matches specifier count?

  ### From safe functions rules:

  - No memset/memcpy/strcpy/strcat/sprintf? Must use _s variants
  - destMax set correctly? Safe function return values checked?

  ### From outparam_rollback_rules.md (if applicable):

  - Output params written before failure? Rolled back on failure?

  ### From global_instance_concurrency_rules.md (if applicable):

  - Global variable access protected by lock?
  - Lock pair correct? No lock nesting deadlock risk?
  - No UAF / half-init / TOCTOU?

  ### From AGENTS.md §4 (prohibited changes):

  - Did not modify existing pkg_inc/*.h definitions (only additions)?
  - Did not modify existing message IDs?

  ## OUTPUT FORMAT

  | Rule Source | Item | Result | File:Line | Issue | Fix Suggestion |

  ## MUST DO

  - Read ALL rule files in STEP 0 before reading code
  - Check every changed file against every applicable rule
  - Output specific File:Line for each ❌ FAIL
  - Output fix suggestion for each ❌ FAIL

  ## MUST NOT DO

  - Do not modify code
  - Do not skip any rule category
  - Do not mark PASS without actually checking against the rule text

  """
)

```

**流程：sub-agent 输出有 ❌ → 编码者修复 → 重新分派 sub-agent 直到全 ✅ → 才能提交 PR。**

> **规则来源分层：**

> 1. AGENTS.md — 驱动模块级约束
> 2. gitcode-pr-review/references/driver_coding_standards.md — 通用 C 编码规范 + 安全编程
> 3. gitcode-pr-review/references/*_rules.md — 专项规则（按需加载）
