# CK_DOC_GATE 补充规则

## 规则 1：交叉引用必须跟进读，不能当黑盒

文档中提到其他模块时（如 smm_design.md 提到"smm_pa_get → 通过 ops 获取源 PA（pmq 注册的回调）"），**必须跟进读被引用模块的 design.md**（如 `pmq_design.md`）。不能把被引用模块当黑盒——"它负责查 PA，我不用知道怎么查"。

> **教训**：smm_design.md 交叉引用了 pmq ops，但没跟进读 pmq_design.md → 不知道 `pmq_local_pa_get` 用 `src_info->tgid` 查源进程 mm → 三次误判"改 udevid 不行"

## 规则 2：调用链上的模块文档都要读，不只读"感觉相关"的

判断"相关"的标准是**"是否在调用链上"**，不是"是否与需求关键词直接匹配"。

> **教训**：需求是"页表同步"→只读 task_group/smm/umc 文档→觉得 pmq"PA 查询和同步无关"没读→但 smm_pa_get→pmq_local_pa_get 在同步调用链上→pmq_design.md 恰好有"pmq_local_pa_get 用 tgid 查源进程 mm"的答案
>
> **正确做法**：画出消息完整路径（如 task_group → smm_client → umc → um_handle → esched → device → smm → pmq），路径上每个模块的 design.md 都读

## 确认链路

当需要确认某模块的行为时，按以下顺序：

1. 读被引用模块的 design.md → 有答案？→ 用文档结论
2. 文档没覆盖？→ 读源码确认 → 有源码？→ 从源码确认
3. 源码也不在本仓？→ **告知人工处理，不假设实现**
