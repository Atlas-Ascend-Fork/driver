# UVM developer demo

## 描述

本样例展示了 UVM的基本使用方法。UVM（Unified Virtual Memory）是昇腾 AI 处理器的内存管理组件，作为原 SVM 模块的功能增强，旨在构建跨主机端与设备端的统一地址空间，支持透明的内存分配与释放逻辑。

样例中包含两个测试用例：
- st_uvm_test_001：演示完整的 UVM 使用流程，包括内存申请释放、缺页中断、属性设置、内存数据预取、内存初始化、内存属性查询、内存拷贝所有基本功能。
- st_uvm_test_002：src/ascend_hal/svm目录下[README](../../../../src/ascend_hal/svm/README.md)中内存预取功能的业务使用流程的代码演示。

## 支持的产品型号

当前只支持Ascend910B/Ascend910\_93硬件形态。

## 编译运行

环境安装详情以及运行详情请见example目录下的[README](../../../README.md)。

## CANN Driver API

在该example中，涉及的关键功能点及其关键接口，如下所示：

- 内存申请与释放
    - 调用 halMemAlloc 接口配合 MEM_UVM 标志申请 UVM 内存，此接口仅预留虚拟地址空间，不立即分配物理页。
    - 调用 halMemFree 接口释放 UVM 内存，同步释放虚拟地址与物理地址，并取消关联的页表映射。
- 缺页中断
    - UVM 内存采用延迟分配机制，当 CPU 或 NPU 首次访问未映射的虚拟地址时，系统触发缺页中断，动态分配物理页并建立页表映射。样例中通过对申请的 UVM 内存进行首次读写来触发缺页中断。
- 内存属性设置
    - 调用 halMemManagedAdvise 接口设置或取消 UVM 内存段的特定属性，详细说明请参考[功能介绍](../../../../src/ascend_hal/svm/README.md)中的"内存属性设置"章节。
- 内存数据预取
    - 调用 halMemManagedPrefetch 接口将内存数据显式预取到目标位置。
- 内存初始化
    - 调用 drvMemsetD8 接口对内存进行按字节初始化。
- 内存属性查询
    - 调用 halMemManagedRangeGetAttributes 接口查询内存属性。
- 内存拷贝
    - 调用 drvMemcpy 接口进行内存数据拷贝。
