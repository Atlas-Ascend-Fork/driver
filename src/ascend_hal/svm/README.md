# SVM

## 整体介绍

SVM (Shared Virtual Memory) 是昇腾 AI 处理器平台中的内存管理模块，用于高效管理设备侧内存。其主要功能包括内存的初始化、申请、释放、拷贝、查询和共享等，并向上层模块（如 Runtime）提供 HAL 接口。

<center>
    <img src="../../../docs/zh/figures/SVM_memory_sharing_process.png" alt="SVM模块依赖" width=70%/>
</center>

## SVM 初始化

SVM 接口 ：```halMemAgentOpen```
<br/>
APP 进程在初始化时会调用上层业务的初始化接口 ```aclrtSetDevice```，其中包含 SVM 模块的初始化流程。该流程主要包括：初始化 SVM 的模块管理结构体，并完成 Host 侧与 Device 侧 SVM 模块之间的交互。

## 内存申请/释放/赋值

SVM 接口：```halMemAlloc```/```halMemFree```/```drvMemsetD8```
<br/>
从预留的虚拟地址范围内分配并映射（通过 ```mmap```）一段虚拟地址，同时进入内核态申请物理页，并建立相应的页表项。释放时，则依次解除页表映射、释放物理页，并将虚拟地址归还至预留范围。内存赋值将指定大小的设备内存设置为给定的值。

此外，驱动还提供了 VMM 接口，允许开发者分别申请虚拟地址和物理地址，并根据需要动态建立它们之间的映射关系。
<br/>

- VMM 虚拟地址申请/释放
<br/>

SVM 接口 ：```halMemAddressReserve```/```halMemAddressFree```
<br/>
从预留的地址范围内分配并映射（通过 ```mmap```）一段虚拟地址，此时尚未真正分配物理页；释放时仅回收该虚拟地址。

- VMM 物理地址申请/释放
<br/>

SVM 接口 ：```halMemCreate```/```halMemRelease```
<br/>
在设备侧分配指定属性和大小的物理内存，并返回一个不透明的通用内存分配句柄，作为后续内存映射的引用标识；释放时，则释放该句柄所对应的物理内存。

- VMM 映射/解映射虚拟地址到物理地址
<br/>

SVM 接口 ：```halMemMap```/```halMemUnmap```
<br/>
建立或解除传入的虚拟地址与物理地址之间的映射关系。

## 内存查询

内存查询支持指定地址的内存属性查询和内存信息查询。

- 内存属性查询
<br/>

SVM 接口 ：```drvMemGetAttribute```
<br/>
获取传入虚拟地址的内存属性、物理页粒度等属性。

- 内存信息查询
<br/>

SVM 接口 ：```halMemGetInfo```
<br/>
查询设备侧的物理内存信息。

## 内存拷贝

SVM 接口 ：```halMemcpy```
<br/>
支持在主机与设备之间，或不同设备间进行内存数据传输。例如，在执行 H2D（Host-to-Device）传输时，会将数据从主机内存同步拷贝到设备内存。该同步拷贝操作是阻塞式的——调用方会一直等待，直到全部数据拷贝完成，才会返回并继续执行后续代码。

## 内存共享

内存共享支持不同设备间的内存共享，以及主机（Host）与设备（Device）之间的内存共享。

- 设备间内存共享
<br/>

SVM 接口 ：
```halShmemCreateHandle```/```halShmemDestroyHandle```
<br/>
```halShmemOpenHandleByDevId```/```halShmemCloseHandle```
<br/>
进程 1 创建一个指向共享设备内存的句柄；进程 2 通过解析该句柄，将其映射为本进程内的设备内存地址，从而实现对设备侧同一块物理内存的共享访问。销毁句柄和解除映射则分别执行相应的反向操作。

- host 和 device 间内存共享
<br/>

SVM 接口 ：```halHostRegister```/```halHostUnregister```
<br/>
将对端（Host 侧或 Device 侧）的内存对应注册到本端（Device 侧或 Host 侧），使本端能够直接访问或修改对端内存中的数据。

## SVM 在业务中的使用流程

#### 申请拷贝功能的业务使用流程

- 应用场景
<br/>

在算子运行前，其 Host 侧进程需先初始化 SVM 模块，然后调用 ```halMemAlloc``` 接口分别申请 Host 内存和 Device 内存，并获取对应的内存地址；随后，通过内存拷贝接口将待计算的数据从 Host 侧传输至 Device 侧。

- 业务调用 SVM 接口流程

1. 调用 ```halMemAlloc``` 接口申请 Host 内存，获取 Host 内存地址。
2. 调用 ```halMemAlloc``` 接口申请 Device 内存，获取 Device 内存地址。
3. 调用 ```halMemcpy``` 接口，将数据从 Host 内存同步拷贝至 Device 内存，完成数据搬运。

<center>
    <img src="../../../docs/zh/figures/SVM_application_copy_process.png" alt="SVM模块申请拷贝流程" width=70% />
</center>

#### 内存共享功能的业务使用流程

- 应用场景
<br/>

业务模块在 Host 侧启动两个应用进程：host_app 进程 0 和 host_app 进程 1，分别对应设备 dev0 和 dev1。
<br/>
在算子执行过程中，dev0 将已完成的计算结果写入其本地设备内存。若后续计算需由 dev1 执行，则 dev1 可通过上述共享内存机制直接访问 dev0 的运算结果，无需经过 Host 中转或显式的数据拷贝，从而显著提升算子流水线的执行效率。

- 业务调用 SVM 接口流程

1. host_app 进程 0 调用 ```halMemAlloc``` 申请 dev0 上的设备内存，并通过 ```halShmemCreateHandle``` 接口创建该内存的共享句柄，随后将该句柄传递给 host_app 进程 1。
2. host_app 进程 1 调用驱动接口 ```halShmemOpenHandleByDevId``` 打开该共享句柄，驱动返回指向共享内存的指针，从而完成对 dev0 内存的映射。

<center>
    <img src="../../../docs/zh/figures/SVM_memory_sharing_process.png" alt="SVM模块内存共享流程" width=70% />
</center>

#### VMM 功能的业务使用流程

- 应用场景
<br/>

基于 VMM 功能，业务模块可一次性申请所需的虚拟地址和物理地址，并根据实际需求动态建立映射关系，重复使用同一块物理内存，从而有效减少因物理内存频繁切分而导致的内存碎片。

- 业务调用 SVM 接口流程
<br/>

1. 调用```halMemAddressReserve```申请虚拟地址。
2. 调用```halMemCreate```申请物理地址。
3. 调用```halMemMap```建立虚拟地址到物理地址的映射。

<center>
    <img src="../../../docs/zh/figures/SVM_VMM_process.png" alt="SVM模块VMM流程" width=70% />
</center>

# UVM

## 整体介绍

UVM（Unified Virtual Memory）是昇腾 AI 处理器的内存管理组件，作为原 SVM 模块的功能增强，旨在构建跨主机端与设备端的统一地址空间，支持透明的内存分配与释放逻辑。

其核心设计原则在于屏蔽物理存储介质的差异性：用户程序仅需维护单一的虚拟地址视图，系统根据访问模式实现数据在 DDR（Double Data Rate）与 HBM（High Bandwidth Memory）间的自动或半自动平滑迁移，并支持零拷贝直接访问。

UVM主要聚焦以下三个功能点：

- 易用性：CPU 和 NPU 可以直接访问统一内存，无需手动调用内存拷贝接口。
- 支持卡侧内存超分：通过冷热识别和内存换出机制，突破单卡物理内存限制，使单个 NPU 能够处理超大规模数据集。
- 高性能：支持提前异步预取数据到需要目标设备，将数据访问和传输任务重叠，实现流水线并行。

<center>
    <img src="../../../docs/zh/figures/UVM_module_dependency.png" alt="UVM模块依赖" width=70%/>
</center>

## 使用限制
当前只支持Ascend910B/Ascend910\_93硬件形态。

## 内存申请释放

UVM 接口：halMemAlloc/halMemFree

内存申请和释放接口复用 SVM 接口实现 UVM 统一虚拟地址管理，核心流程如下：

调用 halMemAlloc 时配合 MEM_UVM 标志，表示申请 UVM 内存。

- 延迟分配：系统仅预留虚拟地址空间，不立即触发物理页分配。
- 按需寻址：物理内存的申请推迟至首次读写访问。此时通过硬件触发 Page Fault，内核动态分配物理页（支持大页）并建立页表映射。
- 资源回收：调用 halMemFree 同步释放虚拟地址与物理地址，并取消关联的页表映射，刷新 TLB（Translation Lookaside Buffer）。

## 内存属性设置

UVM 接口：halMemManagedAdvise 

支持设置或者取消一段 UVM 内存段的特定属性，以优化数据布局：

- READ_MOSTLY：设置内存多副本属性，通常称为 read mostly 属性，适合读多写少场景。该属性通过在 Host 侧和多个 Device 侧同时创建只读副本（虚拟地址共享多个物理副本），旨在减少缺页中断引起的性能开销。其优势在于除首次访问外，各对象访问均无需重新建立映射；但代价较高，一旦发生写操作，除写对象外的所有副本将失效并取消该属性。默认情况下，设置此项会忽略 location 参数并在 Host 侧建立初始副本。
- UNSET_READ_MOSTLY：取消多副本属性。系统仅保留 location 指定位置的副本（若 location 无效则默认保留 Host 侧副本），并释放其他位置的物理内存。
- PREFER_LOCATION：设置内存首选位置属性，通常称为 preferred location 属性，表示访问该内存段内存的首选位置是 Host 侧或 Device 侧，用于提前分配物理内存并建立页表映射，从而避免运行时缺页中断。
在设置preferred location属性时，需要注意 read mostly 属性是否被设置：
  - read mostly 属性未设置时：若在首选位置已有映射则直接返回；若在非首选位置已有映射则返回报错；若都没有映射则申请物理内存、建立 readwrite 属性的页表映射后再返回。
  - read mostly 属性已设置时：若将首选位置设置在 Host 侧，则接口内部无需操作直接返回；若将首选位置设置在某个指定的 Device 侧，则接口内部会检查指定 Device 侧是否有副本，如果有，则直接返回，如果没有，则建立只读副本后再返回。
- UNSET_PREFER_LOCATION：取消设置 preferred location 位置。
- ACCESS_BY_LOCATION：设置远端映射属性，通常称为 access by 属性，建立远端映射，暗示指定设备将访问该段内存。该属性需与首选位置属性配合使用：当两者指定的 location 相同时，系统会提前申请物理内存，建立页表映射，从而避免在首选位置访问内存时出现缺页中断的开销。（注：目前A2/A3架构不支持建立远端映射）。
- UNSET_ACCESS_BY_LOCATION：取消设置 access by 属性。

## 缺页中断

UVM 统一虚拟内存的核心功能是基于页中断的按需迁移。当 CPU 或者 NPU 访问未映射或者权限受限的虚拟地址时，系统触发缺页中断，通过数据搬运、页表重建及副本一致性维护，实现跨设备的地址空间统一。

- Host 侧缺页中断处理：包括未映射地址访问导致的缺页，以及在 readmostly 场景下对 Host 侧页面的写入操作。
- Device 侧缺页中断处理：包括对未映射地址访问导致的缺页，以及在 readmostly 场景下对 Device 页面的写入操作。

## 内存超分

UVM 支持 Device 侧内存超分以应对内存 OOM（Out of Memory）风险，通过将非活跃页面置换至 Host 侧，实现逻辑内存容量的扩展。该机制包含两部分：

- 冷热识别：
    - 硬件标识：利用页表项中的 Young Bit 标志位。当 AI CPU 或 AI Core 访问对应的页面时，硬件自动将该位置置为1。
    - 软件扫描：Device 侧内核态监测线程定期对UVM内存页表项的 Young Bit 执行读后清零操作，实现内存页的冷热识别。
    - 动态分级：结合预设的内存水位线，系统在不同水位压力下动态调整扫描频率与热度阈值，实现差异化的超分策略。

- 内存换出：通过 DMA（Direct Memory Access） 引擎将识别出的冷数据从 Device 侧迁移至预分配的 Host 侧映射区，释放 Device 侧内存。

## 内存数据预取

UVM 接口：halMemManagedPrefetch

显示将数据预取到目标设备：

- 减少性能抖动：避免 Device 侧因触发大量缺页中断而导致严重的性能抖动。
- 流水线并行：结合 Runtime 层实现异步预取接口。在 Device 侧处理当前批次数据的同时，异步预取下一批次数据。
- 批量传输：相比逐页搬运，大块数据的异步预取能更好地跑满 PCie 或者 UB 带宽。

## UVM在业务中使用流程

#### 内存预取功能的业务使用流程

- 应用场景
<br/>

基于 UVM 功能，针对读多写少的场景，比如需要将大模型的权重参数传输到多张卡上做并行推理任务，业务模块可申请所需的虚拟地址，设置 Readmostly 属性，Host 侧申请物理内存并建立映射，通过内存预取将数据传输到 Device 侧。

- 业务调用 UVM 接口流程
<br/>

1. 调用 ```halMemAlloc``` 接口申请 Host 侧虚拟内存，获取 Host 侧虚拟内存地址。
2. 调用 ```halMemManagedAdvise``` 接口设置 READ_MOSTLY 属性，申请物理内存并建立页表映射。
3. 调用 ```halMemManagedPrefetch``` 接口触发预取任务，将数据从 Host 侧迁移到 Device 侧。

- UVM 接口调用流程实现用例
<br/>

[UVM 测试文件](../../../examples/resmng/uvm/developer_demo/uvm_main.c)
<br/>
业务流程测试函数：st_uvm_test_002

<center>
    <img src="../../../docs/zh/figures/UVM_memory_prefetch_process.png" alt="UVM模块内存预取流程" width=70% />
</center>

# SOMA
## 整体介绍
SOMA (Stream-Oriented Memory Allocator) 是 SVM（Shared Virtual Memory）模块下的流式异步内存管理子模块，用于面向流（Stream）执行场景高效管理设备侧内存资源。其主要功能包括内存池的创建与销毁、异步内存申请与释放、内存池属性设置与查询等，并向上层模块（如 Runtime）提供高性能、低开销的异步内存管理能力。
<center>
    <img src="../../../docs/zh/figures/SOMA_module_dependency.png" alt="SOMA模块依赖" width=70%/>
</center>

## 使用限制
- SOMA特性无法通过主机侧调用 HAL 接口使能，只能通过 Runtime ACL 接口使能，使用样例参考[SOMA Runtime example](https://gitcode.com/cann/runtime/blob/master/example/3_memory_advanced/memory_pool)。
- 当前只支持Ascend950 PCIE硬件形态。

## SOMA 内存池创建销毁
SOMA 接口：`halMemPoolCreate` / `halMemPoolDestroy`  

通过 `halMemPoolCreate` 可以在设备上创建一个内存池管理结构，用于管理后续的异步内存申请和释放。内存池创建时可以设置内存池的id、内存池的最大容量等，但不会立即分配物理内存。  

通过 `halMemPoolDestroy` 可以销毁指定的内存池管理结构，确保所有由该池分配的内存块被正确释放，并将相关资源归还给系统，从而避免内存泄漏。

## SOMA 异步内存申请/释放
SOMA 接口：`halMemPoolMalloc` / `halMemPoolFree`  

`halMemPoolMalloc` 是 device 侧提供给 AI CPU 调用的内存申请接口，用于从指定内存池中申请一段设备内存。调用时需要指定目标内存池、虚拟地址、申请大小以及内存申请策略。函数会根据内存池管理结构分配物理页，并建立对应的页表映射，使申请得到的内存可以直接用于后续计算。

`halMemPoolFree` 用于将一段设备内存归还到内存池。调用时需要指定目标内存池、虚拟地址、释放大小以及释放策略。释放后的内存不会立即归还给操作系统，而是保留在内存池中等待后续复用，从而提高内存利用率和分配效率。

## SOMA 异步申请内存地址的同步释放
同步内存释放接口：`halMemFree`  
<br/>
SOMA 内存地址的同步释放复用 SVM 的 `halMemFree` 接口。当调用 `halMemFree` 释放一个由 SOMA 内存池异步申请得到的虚拟地址时，驱动通过 `svm_va_is_soma` 判断该地址的内存来源为 SOMA 内存池（`SVM_POOL_MEM`），进而转入 `soma_mem_sync_free` 路径完成同步释放。

与常规内存释放不同，SOMA 内存地址的同步释放流程如下：
1. 通过地址解析出对应的 SOMA 内存池句柄，并获取该地址已配置的内存大小。
2. 通过 `mem_pool_client_mem_free` 向 Device 侧（AI CPU）发送同步释放事件，清除该块内存在内存池中对应的 used/PA 位图记录，并解除物理页映射（depopulate）。
3. 调用 `soma_mem_free_cfg` 注销该地址在内核态的 KSVMM 映射关系，完成资源回收。

通过上述同步释放机制，业务可以通过统一的 `halMemFree` 接口释放 SOMA 内存，无需区分地址的来源，简化了内存管理流程并避免内存泄漏。

## SOMA 内存池属性设置与查询
SOMA 接口：`halMemPoolSetAttr` / `halMemPoolGetAttr`  

通过 `halMemPoolSetAttr` 可以设置内存池的属性，如内存池水线值等。调用时需要指定目标内存池和属性键值对。  

通过 `halMemPoolGetAttr` 可以查询内存池的当前属性值。调用时需要指定目标内存池和属性键，返回对应属性值。  

## SOMA 内存复用（单流、跨流事件、机会主义、隐式依赖）
SOMA 接口：`halMemPoolAsyncConfig` / `halMemPoolMalloc`  
<br/>
为提升内存复用效率，SOMA 在异步申请内存的基础上新增了单流、跨流事件、机会主义、隐式依赖等多种内存复用策略，使同一块设备内存能够在不同算子和流之间复用，降低物理内存申请的开销与碎片。

内存复用整体分为两步：
- **配置阶段**（`halMemPoolAsyncConfig`）：在算子分发前对指定虚拟地址进行申请/释放预处理，将虚拟地址与内存池绑定。新增的 `SOMA_MEM_WAITING` flag 与 `SOMA_MEM_ALLOC` 在配置阶段逻辑一致，均登记虚拟地址到内存池的映射关系；真正的复用/等待语义在申请阶段由 Device 侧策略决定。
- **申请阶段**（`halMemPoolMalloc`）：调用时需要指定内存申请策略，新增的内存复用策略包括：
    - `SOMA_MEM_ALLOC_REUSE_POLICY`（Reuse）：机会主义复用。仅在目标内存块已满足可复用条件时快速复用（fail_fast），适用于单流场景下内存已释放、可直接复用的情形。
    - `SOMA_MEM_ALLOC_WAITING_POLICY`（Awaiting reuse）：带等待的隐式依赖复用。当目标内存块正处于被使用（未释放）状态时，申请任务会按照一定的退避策略等待其释放后再复用（retry），用于跨流事件、隐式依赖等场景，等待既定的依赖（如跨流事件、上一个算子结束）释放内存。
    
    配合上述策略，SOMA 支持单流（同一流内前一算子释放后直接复用）、跨流事件（通过跨流事件同步依赖，等待对端流释放内存）、机会主义（有则用、无则快速失败）以及隐式依赖（依据执行依赖隐式等待内存可复用）等内存复用能力，从而显著提升内存利用率，减少因频繁向系统申请物理页带来的开销。

释放侧同样支持显式释放与隐式释放策略（`SOMA_MEM_FREE_EXP_POLICY`/`SOMA_MEM_FREE_IMP_POLICY`），与上述复用策略配合，由业务根据执行语义选择合适的申请/释放策略组合。

## SOMA 异步申请内存的同步拷贝与赋值
SOMA 接口：`drvMemcpy` / `drvMemsetD8`  
<br/>
SOMA 异步申请得到的内存虚拟地址在建立映射后，可以直接作为普通设备内存使用，支持同步拷贝与赋值操作：

- **同步拷贝**（`drvMemcpy`）：将指定内存数据从源地址同步拷贝到 SOMA 设备内存（H2D/D2D），或从 SOMA 设备内存拷贝回主机（D2H）。该同步拷贝操作为阻塞式，调用方会一直等待拷贝完成。
- **同步赋值**（`drvMemsetD8`）：将 SOMA 设备内存的指定区域设置为给定的 8 位值，用于内存初始化或清零。

由于 SOMA 内存虚拟地址在申请时会携带同步拷贝（`SVM_FLAG_CAP_SYNC_COPY_EX`）与赋值（`SVM_FLAG_CAP_MEMSET`）等能力，业务无需额外步骤即可对 SOMA 异步内存执行同步拷贝与赋值，实现"异步申请、同步使用"的灵活内存操作。

**SOMA 内存同步拷贝支持场景**：根据源/目标内存所在的位置，SOMA 同步拷贝覆盖非跨卡（同卡）与跨卡两类场景。

- **非跨卡（同卡内）拷贝场景**
  - Host 锁页内存 与 SOMA 内存 之间（H2D/D2H）：将数据在 Host 锁页内存与 SOMA 设备内存间同步搬运，适用于需要稳定映射、支持 DMA 直传的锁页缓冲。
  - Host 非锁页内存 与 SOMA 内存 之间（H2D/D2H）：将数据在 Host 非锁页内存与 SOMA 设备内存间同步搬运，适用于常规用户态缓冲。
  - Device 内存 与 SOMA 内存 之间（D2D）：在同一设备上的普通设备内存与 SOMA 内存之间同步拷贝。
  - SOMA 内存 到 SOMA 内存（D2D）：在同一设备上的两个 SOMA 内存之间同步拷贝，例如将算子的输出搬移到下一算子的输入。

- **跨卡拷贝场景**
  - SVM 内存 与 SOMA 内存 之间（D2D）：在不同设备上的普通 SVM 设备内存与 SOMA 内存之间同步拷贝。
  - SOMA 内存 到 SOMA 内存（D2D）：在不同设备上的两个 SOMA 内存之间同步拷贝，用于跨卡数据交换与流水线并行。
  - （SOMA 参与的跨卡拷贝覆盖不同设备间的数据同步搬运，具体支持的方向以平台提供的 D2D 拷贝能力为准。）

上述场景中的同步拷贝操作均为阻塞式，调用方会一直等待拷贝完成后才返回。赋值操作 `drvMemsetD8` 则对 SOMA 设备内存的指定区域设置为给定值，用于初始化或清零。

## SOMA在业务中使用流程
#### 内存池异步申请与释放的业务使用流程
- **应用场景**  
<br/>
在 AI 模型训练或推理过程中，多个算子在不同流中并行执行，需要频繁申请和释放设备内存。若每次都向系统申请物理页，会产生大量开销和碎片。SOMA 通过内存池和异步机制，使内存申请、释放高效进行，从而提升流水线执行性能。

- **业务调用 SOMA 接口流程**  
1. 初始化内存池，调用 `halMemPoolCreate` 创建内存池管理结构。  
2. 在算子执行前，调用 `halMemPoolMalloc` 异步申请设备内存，获取虚拟地址。  
3. 算子完成计算后，调用 `halMemPoolFree` 将设备内存归还内存池，释放资源。  

<center>
    <img src="../../../docs/zh/figures/SOMA_malloc_and_free_process.png" alt="SOMA模块复用策略流程" width=100% />
</center>

#### SOMA 同步释放功能的业务使用流程

- **应用场景**  
<br/>
在算子执行完成、需要回收 SOMA 设备内存时，业务可以直接通过统一的 `halMemFree` 接口释放 SOMA 异步申请得到的内存地址，驱动内部自动识别 SOMA 来源并执行同步释放，简化释放流程。

- **业务调用 SOMA 接口流程**  
1. 调用 `halMemPoolCreate` 创建内存池管理结构。
2. 调用 `halMemPoolMalloc` 异步申请设备内存，获取虚拟地址。
3. 算子计算完成后，调用 `halMemFree` 直接释放该 SOMA 内存虚拟地址，驱动识别 SOMA 来源后执行同步释放，回收内存。

#### SOMA 内存复用功能的业务使用流程

- **应用场景**  
<br/>
在 AI 推理流水线中，同一块设备内存可被 单流 / 跨流事件 / 机会主义 / 隐式依赖 等多种方式复用于不同算子，从而减少物理内存申请开销，提升内存利用率和流水线效率。

- **业务调用 SOMA 接口流程**  
1. 调用 `halMemPoolCreate` 创建内存池管理结构。
2. 算子分发前，调用 `halMemPoolAsyncConfig` 对待分配的虚拟地址进行申请配置（`SOMA_MEM_ALLOC`/`SOMA_MEM_WAITING`），登记到内存池。
3. 在算子执行前，调用 `halMemPoolMalloc` 并指定复用策略（`SOMA_MEM_ALLOC_REUSE_POLICY`/`SOMA_MEM_ALLOC_WAITING_POLICY`）异步申请设备内存；根据单流、跨流事件、机会主义、隐式依赖等场景选择相应策略，复用已释放或依赖满足后释放的内存。
4. 算子完成计算后，根据语义选择显式释放（`SOMA_MEM_FREE_EXP_POLICY`）或隐式释放（`SOMA_MEM_FREE_IMP_POLICY`）将设备内存归还内存池，等待后续复用。

#### SOMA 内存同步拷贝与赋值功能的业务使用流程

- **应用场景**  
<br/>
SOMA 异步申请得到的内存地址可直接用于数据搬运与初始化：通过同步拷贝将待计算数据从 Host 侧写入 SOMA 设备内存，或通过同步赋值对 SOMA 内存进行初始化/清零。

- **业务调用 SOMA 接口流程**  
1. 调用 `halMemPoolCreate` 创建内存池管理结构。
2. 调用 `halMemPoolMalloc` 异步申请设备内存，获取虚拟地址。
3. 调用 `drvMemcpy` 将数据从 Host 内存同步拷贝至 SOMA 设备内存，或将 SOMA 设备内存数据拷贝回 Host。
4. 调用 `drvMemsetD8` 将 SOMA 设备内存区域的指定大小设置为给定值，完成内存初始化或清零。
5. 算子计算完成后，调用 `halMemFree` 或 `halMemPoolFree` 释放 SOMA 内存。