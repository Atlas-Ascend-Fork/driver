# UVM developer demo

## Description

This example demonstrates the basic usage of UVM (Unified Virtual Memory). UVM is a memory management component of the Ascend AI processor. As a functional enhancement of the original SVM module, it aims to build a unified address space across the host and device sides, supporting transparent memory allocation and deallocation logic.

The example contains two test cases:
- st_uvm_test_001: Demonstrates the complete UVM usage flow, including memory allocation and release, page faults, attribute setting, memory data prefetch, memory initialization, memory attribute querying, and memory copy.
- st_uvm_test_002: Code demonstration of the business usage flow for the memory prefetch feature described in the [README_en](../../../../src/ascend_hal/svm/README_en.md) under the src/ascend_hal/svm directory.

## Supported Product Models

Currently only supports Ascend910B/Ascend910\_93 hardware.

## Compilation and Running

For environment installation details and running details, refer to the [README_en](../../../README_en.md) in the examples directory.

## CANN Driver API

In this example, the key features and key interfaces involved are shown below:

- Memory Allocation and Release
    - Call halMemAlloc interface with the MEM_UVM flag to allocate UVM memory. This interface only reserves the virtual address space without immediately allocating physical pages.
    - Call halMemFree interface to release UVM memory, synchronously releasing both virtual and physical addresses and removing associated page table mappings.
- Page Fault
    - UVM memory uses a lazy allocation mechanism. When the CPU or NPU first accesses an unmapped virtual address, the system triggers a page fault, dynamically allocating physical pages and establishing page table mappings. In the example, page faults are triggered by performing the first read/write operation on the allocated UVM memory.
- Memory Attribute Setting
    - Call halMemManagedAdvise interface to set or unset specific attributes for UVM memory segments. For details, please refer to the "Memory Attribute Setting" section in the [feature introduction](../../../../src/ascend_hal/svm/README_en.md).
- Memory Data Prefetch
    - Call halMemManagedPrefetch interface to explicitly prefetch memory data to a target location.
- Memory Initialization
    - Call drvMemsetD8 interface to initialize memory byte by byte.
- Memory Attribute Querying
    - Call halMemManagedRangeGetAttributes interface to query memory attributes.
- Memory Copy
    - Call drvMemcpy interface to copy memory data.

