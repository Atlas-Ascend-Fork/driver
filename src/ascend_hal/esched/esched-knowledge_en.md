# Esched Knowledge Base

## Source Code Scope

### HAL User-Mode
- Path: `src\ascend_hal\esched`
- Subdirectory list and function descriptions

### SDK Kernel-Mode
- Path: `src\sdk_driver\esched`

## API Description
- Total number of external APIs and external entry files
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
Interface declarations are in: pkg_inc\ascend_hal_external.h

- Build CMake description (what host builds, what device builds)
Host build path: src\ascend_hal\esched, src\sdk_driver\esched
Device build path: not visible in the current repository
## Code Visibility
- Only host-side code is visible in the current repository
- How to infer device-side code (message interfaces + host context)
Host sends to device: devdrv_common_msg_send
Host receives device messages: esched_ctrl_msg_recv

## Reference Document Index
- Kernel-mode reference document list (if any)
- User-mode reference document list (if any)
