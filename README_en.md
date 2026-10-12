# driver

English | [简体中文](./README.md)

## 🔥Latest News

- **[2026/06] Added support for the Ascend A5 chip (in UB)**.

- [2026/03] Added support for the Ascend A5 chip (in PCIe).

- [2025/12] The driver project is first released.

## 🚀Overview

The Driver repository code is the driver module of CANN (Compute Architecture for Neural Networks), providing basic driver, resource management, and scheduling functions to enable Ascend chips. The current open-source repository mainly contains three parts as shown in the diagram: DCMI layer (DaVinci Card Management Interface), HAL layer (Hardware Abstraction Layer), and SDK-driver layer (Driver Software Development Kit).

Refer to the [Ascend Community](https://www.hiascend.com/cann) for the position of Driver in the CANN software stack.

<center>
    <img src="./docs/en/figures/Driver_architecture_en.png" alt="Driver Architecture Layer Diagram" />
</center>

## 📖Module Introduction

- [RoCE (RDMA over Converged Ethernet)](./src/ascend_hal/roce/README_en.md): The RoCE module in the Ascend AI processor platform reduces latency and improves data transmission efficiency.
- [SVM (Shared Virtual Memory)](./src/ascend_hal/svm/README_en.md): The memory management module in the Ascend AI processor platform for efficient management of device-side memory.

## ⚡️Quick Start

If you want to quickly experience the invocation and development process of driver, visit the following documents for a simple tutorial.

- [QUICKSTART](./docs/en/QUICKSTART.md): End-to-end quick start guide, including setting up the environment, compilation deployment, source code development, debugging, contribution, and other processes.
- [Reference Examples](./examples/README_en.md): Introduction to basic examples of modules such as device management.

## 📝Directory Structure

Key directory structure is as follows:

```bash
├── build.sh                                       # Project compilation script
├── cmake                                          # Project compilation directory
├── CMakeLists.txt                                 # Project CMakeLists entry
├── CONTRIBUTING.md                                # Community contribution guide
├── docs                                           # Documentation
├── examples                                       # Interface usage examples
├── pkg_inc                                        # Header files provided by this repository
├── LICENSES                                       # License directory for this repository
├── OAT.xml                                        # Configuration script for repository tools to check License compliance
├── README.md
├── scripts                                        # Script directory for this repository
│   ├── package                                    # Build and package related scripts
│   ├── ut                                         # UT cpp coverage generation script
├── SECURITY.md                                    # Project security statement file
├── Third_Party_Open_Source_Software_Notice        # Third-party open-source software notice for this repository
├── src                                            # Driver package source code
│   ├── ascend_hal                                 # HAL layer source code folder
│   │   ├── bbox                                   # Black Box (system last words)
│   │   ├── buff                                   # Inter-process shared memory management
│   │   ├── build                                  # ascend_hal dynamic library compilation script
│   │   ├── comm                                   # Communication host-side <-> device-side communication layer
│   │   ├── dmc                                    # DMC (Device Maintenance Components) device maintenance component
│   │   │   ├── device_monitor                     # DSMI message pathway
│   │   │   ├── dsmi                               # DSMI (Device System Management Interface) device system management interface
│   │   │   ├── logdrv                             # Log
│   │   │   ├── prof                               # Profiling performance collection
|   |   |   ├── prof_sample                        # Profiling host-side collection registration
│   │   │   └── verify_tool                        # Device-side image verification tool
│   │   ├── dms                                    # DMS (Device Management System) device management system
│   │   ├── dpa                                    # DPA (Device Public Adapter) device public adaptation layer
│   │   ├── esched                                 # Event Schedule
│   │   ├── hdc                                    # Host-Device Communication
│   │   ├── inc                                    # HAL layer internal common header file directory
│   │   ├── mmpa                                   # MMPA (Medium Multiple Platform Adaptive) basic system interface library
│   │   ├── msnpureport                            # Device-side debugging information export tool
│   │   ├── pbl                                    # PBL (Public Base Lib) basic public library
│   │   │   ├── uda                                # UDA (Unified Device Access) unified device access
│   │   │   ├── urd                                # URD (User Request Distribute) user request forwarding
│   │   │   ├── commlib                            # Common function library
│   │   │   ├── queryfeature                       # Software feature query for compatibility adaptation
|   |   |   └── ubmm                               # UB Memory Adapter
│   │   ├── queue                                  # Message queue information management
│   │   ├── roce                                   # RoCE (RDMA over Converged Ethernet)
│   │   ├── svm                                    # Shared Virtual Memory
│   │   └── trs                                    # Task Resource Schedule
│   ├── custom                                     # Custom feature source library
│   │   ├── cmake                                  # CMake compilation configuration directory
│   │   ├── dev_prod                               # Device customization management directory
│   │   ├── include                                # Common header file export directory
│   │   ├── lqdrv                                  # Lingqu PCIe fault detection
│   │   ├── ndr                                    # NPU RDMA passthrough feature
│   │   ├── network                                # DCMI network interface implementation
│   │   └── ops_debug                              # Operator debugging directory
│   └── sdk_driver                                 # SDK layer source code folder
│       ├── buff                                   # Inter-process shared memory management
│       ├── comm                                   # Communication host-side <-> device-side communication layer
│       ├── dmc                                    # DMC (Device Maintenance Components) device maintenance component
│       ├── dms                                    # DMS (Device Management System) device management system
│       ├── dpa                                    # DPA (Device Public Adapter) device public adaptation layer
│       ├── dvpp                                   # DVPP (Digital Vision Pre-Processing) digital vision pre-processing module
│       ├── esched                                 # Event Schedule
│       ├── fms                                    # FMS (Fault Management System) fault management system
│       ├── hdc                                    # Host-Device Communication
│   │   ├── inc                                    # SDK layer internal common header file directory
│       ├── kernel_adapt                           # SDK driver code and kernel source adaptation layer
│       ├── pbl                                    # PBL (Public Base Lib) basic public library
│       ├── platform                               # Chip resource (interrupt, reserved memory, and so on) storage repository
│       ├── queue                                  # Message queue information management
|       ├── seclib                                 # Secure Library common security function library
│       ├── svm                                    # Shared Virtual Memory
│       ├── ts_agent                               # TS (Task Schedule) agent driver source code
│       ├── trsdrv                                 # TRS (Task Resource Schedule) software sqcq communication, mailbox message feature
│       │   ├── trs                                # Task Resource Schedule
│       │   └── trsbase                            # Task Resource Schedule base layer
│       ├── vascend                                # Ascend computing power segmentation feature
│       ├── vmng                                   # Virtual Machine Manager device virtualization management
│   │   ├── vnic                                   # VNIC (Virtual Network Interface Card) virtual network interface card
│   │   └── vpc                                    # VPC (Virtual Physical Communication) physical machine and virtual machine communication
└── test                                           # UT case file directory
```

## 🔍FAQ

- [FAQ](./docs/en/FAQ.md): Summary of source code compilation, installation deployment, and other problems.

## Related Information

- [Contributing Guide](./CONTRIBUTING_en.md)
- [Security Statement](./SECURITY_en.md)
- License

&emsp;&emsp;&emsp;[CANN Open Software License Agreement Version 2.0](./LICENSES/CANN-V2.0)

&emsp;&emsp;&emsp;[GNU GENERAL PUBLIC LICENSE Version 2](./LICENSES/GPL-V2.0)
