# Esched 知识库

## 源码范围

### HAL 用户态
- 路径: `src\ascend_hal\esched`
- 子目录清单 + 功能说明

### SDK 内核态
- 路径: `src\sdk_driver\esched`

## API 说明
- 对外 API 总数 + 对外入口文件
halEschedAttachDevice
halEschedDettachDevice
halEschedRegisterFinishFunc
halEschedWaitEvent
halEschedThreadSwapout
halEschedThreadGiveup
halEschedGetEvent
halEschedSubmitEvent
halEschedSubmitEventBatch
halEschedSubmitEventToThread
halEschedSubmitEventEx
halEschedRegisterAckFunc
halEschedAckEvent
halEschedSubscribeEvent
halEschedSetGrpEventQos
halEschedCreateGrp
halEschedCreateGrpEx
halEschedQueryInfo
halEschedQueryInfoEx
halEschedSetPidPriority
halEschedSetEventPriority
halEschedDumpEventTrace
halEschedTraceRecord
接口声明在：pkg_inc\ascend_hal_external.h

- 构建 cmake 说明（host 编译哪些、device 编译哪些）
host编译路径：src\ascend_hal\esched,src\sdk_driver\esched
device编译路径：当前仓不可见
## 代码可见性
- 当前仓库仅可见host侧代码
- Device 侧代码如何推断（消息接口 + Host 上下文）
host发送到device： devdrv_common_msg_send
host接收device消息： esched_ctrl_msg_recv

## 参考文档索引
- 内核态参考文档列表（如有）
- 用户态参考文档列表（如有）