# Table of Contents

- [Table of Contents](#table-of-contents)
- [Introduction](#introduction)
  - [Component Introduction](#component-introduction)
  - [Application Scenarios](#application-scenarios)
    - [NPU Direct Async Scenarios](#npu-direct-async-scenarios)
    - [Hyper RoCE Scenarios](#hyper-roce-scenarios)
- [Installation](#installation)
  - [Install through Source Compilation](#install-through-source-compilation)
- [Usage](#usage)
- [API Reference](#api-reference)
  - [Common Interfaces](#common-interfaces)
    - [ibv\_extend\_get\_version](#ibv_extend_get_version)
    - [ibv\_extend\_check\_version](#ibv_extend_check_version)
  - [North-bound Interfaces (Context Management)](#north-bound-interfaces-context-management)
    - [ibv\_open\_extend](#ibv_open_extend)
    - [ibv\_close\_extend](#ibv_close_extend)
  - [North-bound Interfaces (NPU Direct Async)](#north-bound-interfaces-npu-direct-async)
    - [ibv\_query\_device\_extend](#ibv_query_device_extend)
    - [ibv\_create\_qp\_extend](#ibv_create_qp_extend)
    - [ibv\_create\_cq\_extend](#ibv_create_cq_extend)
    - [ibv\_create\_srq\_extend](#ibv_create_srq_extend)
    - [ibv\_destroy\_qp\_extend](#ibv_destroy_qp_extend)
    - [ibv\_destroy\_cq\_extend](#ibv_destroy_cq_extend)
    - [ibv\_destroy\_srq\_extend](#ibv_destroy_srq_extend)
  - [North-bound Interfaces (Hyper RoCE)](#north-bound-interfaces-hyper-roce)
    - [ibv\_modify\_qp\_extend](#ibv_modify_qp_extend)
    - [ibv\_query\_qp\_extend](#ibv_query_qp_extend)
  - [Northbound Interface (Hyper RoCE Negotiation)](#northbound-interface-hyper-roce-negotiation)
    - [ibv\_query\_qp\_supported\_hyroce\_feature](#ibv_query_qp_supported_hyroce_feature)
    - [ibv\_nego\_qp\_hyroce\_feature](#ibv_nego_qp_hyroce_feature)
  - [South-bound Interfaces (Driver Call)](#south-bound-interfaces-driver-call)
    - [verbs\_register\_driver\_extend](#verbs_register_driver_extend)
- [Key Structure Description](#key-structure-description)
  - [Enum Type Definition](#enum-type-definition)
    - [queue\_buf\_dma\_mode](#queue_buf_dma_mode)
    - [doorbell\_map\_mode](#doorbell_map_mode)
    - [memcpy\_direction](#memcpy_direction)
    - [ibv\_qp\_init\_cap](#ibv_qp_init_cap)
    - [ibv\_extend\_device\_cap](#ibv_extend_device_cap)
    - [IBV\_EXTEND\_DRIVER\_VERSION (Driver Operations Interface Version)](#ibv_extend_driver_version-driver-operations-interface-version)
    - [ibv\_hyroce\_feature\_type](#ibv_hyroce_feature_type)
    - [ibv\_hyroce\_feature\_version](#ibv_hyroce_feature_version)
    - [ibv\_lb\_mode](#ibv_lb_mode)
    - [ibv\_qp\_attr\_extend\_mask](#ibv_qp_attr_extend_mask)
  - [Core Data Structures](#core-data-structures)
    - [doorbell\_map\_desc](#doorbell_map_desc)
    - [ibv\_extend\_ops](#ibv_extend_ops)
    - [ibv\_context\_extend\_ops](#ibv_context_extend_ops)
    - [verbs\_device\_extend\_ops](#verbs_device_extend_ops)
    - [ibv\_hyroce\_feature](#ibv_hyroce_feature)
    - [ibv\_mpath\_config](#ibv_mpath_config)
    - [ibv\_ar\_config](#ibv_ar_config)
    - [ibv\_sack\_config](#ibv_sack_config)
    - [ibv\_qp\_attr\_extend](#ibv_qp_attr_extend)
- [FAQ](#faq)
  - [Error when calling ibv\_open\_extend interface to initialize extension library](#error-when-calling-ibv_open_extend-interface-to-initialize-extension-library)

# Introduction

## Component Introduction

ibverbs_extend (hereinafter referred to as ibv_extend) component is an extension library for standard RDMA verbs interfaces, providing capabilities beyond standard ibverbs for upper-layer communication components.

The ibv_extend component achieves north-south decoupling through unified interfaces, as shown in the following figure:

- North-bound shields underlying network card hardware differences, providing transparent access for HCOMM and other communication transport layers.
- South-bound interfaces with 1825, Yunmai, and other network card extension drivers, enabling them to work without sensing upper-layer computing and memory resource differences.

![ibv_extend Logic Diagram](figures/ibv_extend_Logic_Block_Diagram.png)

The ibv_extend component currently supports the following two independent feature modules:

- **NPU Direct Async (NDA)**: Supports RDMA queue resource management on the NPU side, enabling direct data transfer between NPU and network cards
- **Hyper RoCE**: Supports advanced RoCE feature configuration, including load balancing, selective retransmission, and other advanced network communication capabilities

The two feature modules are independent and do not depend on each other:

- For scenarios requiring only NDA features, drivers need only implement V1 version interfaces
- For scenarios requiring only Hyper RoCE features, drivers need to implement V2 version interfaces (V2 includes V1 base interfaces)
- Both features can be used simultaneously; the same extension context created via `ibv_open_extend` provides access to all functionality

The ibv_extend component currently supports the following two independent feature modules:

- **NPU Direct Async (NDA)**: Supports RDMA queue resource management on the NPU side, enabling direct data transfer between NPU and network cards
- **Hyper RoCE**: Supports advanced RoCE feature configuration, including load balancing, selective retransmission, and other advanced network communication capabilities

The two feature modules are independent and do not depend on each other:

- For scenarios requiring only NDA features, drivers need only implement V1 version interfaces
- For scenarios requiring only Hyper RoCE features, drivers need to implement V2 version interfaces (V2 includes V1 base interfaces)
- Both features can be used simultaneously; the same extension context created via `ibv_open_extend` provides access to all functionality

The ibv_extend component provides the following core features:

- Dynamic loading and management of extension drivers
- Creation and destruction of extended QP/CQ/SRQ (NDA feature)
- Modification and query of QP extended attributes (Hyper RoCE feature)
- Support for north-south custom hardware operations through callback function mechanism

## Application Scenarios

### NPU Direct Async Scenarios

- Direct data transfer between NPU and RDMA network card.
- Bypassing Host CPU, direct-drive control plane between NPU and RDMA network card.

![NDA Application Scenario Diagram](figures/NDA_Application_Scenario.png)

### Hyper RoCE Scenarios

- Network communication requiring advanced RoCE protocol features.
- Multi-path load balancing and adaptive routing optimization.
- Selective retransmission for improved network reliability.

![Hyper RoCE Application Scenario Diagram](figures/Hyper_RoCE_Application_Scenario.png)

# Installation

## Install through Source Compilation

1. Obtain source code and enter the component source directory.

    Method 1: Use git clone to get source code.

    ```bash
    git clone https://gitcode.com/cann/driver.git
    cd driver/src/custom/nda/ibv_extend/
    ```

    Method 2: Web page download source code.

    Browse to [driver repository](https://gitcode.com/cann/driver), click "Download zip" button to get the latest source code. After uploading to the server, execute the following command.

    ```bash
    unzip driver-master.zip
    cd driver-master/src/custom/nda/ibv_extend/
    ```

2. Compilation dependencies.

    For openEuler/CentOS/RHEL:

    ```bash
    yum install gcc gcc-c++ make cmake libnl3-devel libudev-devel pkgconfig python3-devel
    ```

    For Ubuntu/Debian:

    ```bash
    apt install gcc g++ make cmake libnl-3-dev libnl-route-3-dev libudev-dev pkg-config python3-dev
    ```

3. Compile and install.

    ```bash
    bash ./build.sh
    ```

    **Table 1** Compilation Parameter Description

    <table>
    <tr>
        <th valign="top" width="18%"><p>Parameter</p></th>
        <th valign="top" width="18%"><p>Long Parameter</p></th>
        <th valign="top" width="33%"><p>Description</p></th>
        <th valign="top" width="31%"><p>Usage Example</p></th>
    </tr>
    <tr>
        <td><p>-t=TYPE</p></td>
        <td><p>--type=TYPE</p></td>
        <td><p>Compile type:</p>
            <ul>
            <li>release: Default value, automatically downloads rdma-core.</li>
            <li>debug: Debug type.</li>
            </ul>
        </td>
        <td><p>Compile in debug mode.</p><code>bash ./build.sh -t=debug</code></td>
    </tr>
    <tr>
        <td><p>-b=PATH</p></td>
        <td><p>--libibverbs-build-dir=PATH</p></td>
        <td><p>Specify pre-compiled rdma-core build directory. Mutually exclusive with <strong>-s</strong> parameter, cannot be used together.</p></td>
        <td><p>Use pre-compiled rdma-core build library.</p><code>bash ./build.sh -b=/path/to/rdma-core/build</code></td>
    </tr>
    <tr>
        <td><p>-s=PATH</p></td>
        <td><p>--libibverbs-source-dir=PATH</p></td>
        <td><p>Specify rdma-core source directory. Mutually exclusive with <strong>-b</strong> parameter, cannot be used together.</p></td>
        <td><p>Use custom rdma-core source code.</p><code>bash ./build.sh -s=/path/to/rdma-core</code></td>
    </tr>
    <tr>
        <td><p>-x=PATH</p></td>
        <td><p>--libboundscheck-build-dir=PATH</p></td>
        <td><p>Specify pre-compiled libboundscheck build directory. Mutually exclusive with <strong>-e</strong> parameter, cannot be used together.</p></td>
        <td><p>Use pre-compiled libboundscheck build library.</p><code>bash ./build.sh -x=/path/to/libboundscheck</code></td>
    </tr>
    <tr>
        <td><p>-e=PATH</p></td>
        <td><p>--libboundscheck-source-dir=PATH</p></td>
        <td><p>Specify libboundscheck source directory. Mutually exclusive with <strong>-x</strong> parameter, cannot be used together.</p></td>
        <td><p>Use custom libboundscheck source code.</p><code>bash ./build.sh -e=/path/to/libboundscheck</code></td>
    </tr>
    <tr>
        <td><p>-h</p></td>
        <td><p>--help</p></td>
        <td><p>Show help information.</p></td>
        <td><p>View help information.</p><code>bash ./build.sh -h</code></td>
    </tr>
    </table>

4. After compilation, check output files.

    ```text
    output/
    ├── lib/
    │    └── libibv_extend.so    # Shared library
    └── include/
    │    └── ibv_extend.h        # Header file
    ```

5. Dynamically specify environment variables at runtime.

    ```bash
    export LD_LIBRARY_PATH=/path/to/ibv_extend/output/lib:$LD_LIBRARY_PATH
    ```

    > **Note:**
    >
    > - /path/to/ is a variable, replace with actual path.
    > - Users can also copy dynamic libraries under output/lib or header files under output/include to other custom paths or system paths for subsequent use.

6. Compile security function library.

    Go to [libboundscheck release page](https://gitcode.com/cann-src-third-party/libboundscheck/releases) to get the latest source code package, for example download [libboundscheck-v1.1.16.tar.gz](https://gitcode.com/cann-src-third-party/libboundscheck/releases/download/v1.1.16/libboundscheck-v1.1.16.tar.gz).

    Compile libc_sec.so dynamic library.

    ```bash
    # Extract to current directory
    tar zxvf libboundscheck-v1.1.16.tar.gz
    # Enter current directory
    cd libboundscheck-v1.1.16
    # Compile
    make -j
    # After successful compilation, dynamic link library will be generated in lib directory, need to rename to libc_sec.so.
    cp lib/libboundscheck.so lib/libc_sec.so
    ```

    Need to depend on libc_sec.so dynamic library during use.

    ```bash
    export LD_LIBRARY_PATH=/path/to/libboundscheck-v1.1.16/lib:$LD_LIBRARY_PATH
    ```

# Usage

The ibv_extend component allows upper-layer communication libraries to dynamically load extension drivers through **configuring environment variables** or **configuration files**.

Before running, you can configure environment variable `export IBV_EXTEND_SHOW_WARNINGS=1` to let ibv_extend component print Warning level logs to console. Warning level logs are not printed by default.

Before running, ensure that libibverbs.so and libibv_extend.so are correctly installed. For specific operations, refer to the table below.

**Table 1** Pre-run Check

<table>
    <tr>
        <th>System Type</th>
        <th>Check/Install libibverbs.so</th>
        <th>Check/Install libibv_extend.so</th>
    </tr>
    <tr>
        <td>openEuler/CentOS/RHEL</td>
        <td>
            <ol>
                <li>Check if libibverbs.so is installed.<br><code>ldconfig -p | grep libibverbs</code><br>If there is output, libibverbs.so is installed, done.<br>If no output, continue.</li>
                <li>Install libibverbs.<br><code>sudo yum install rdma-core</code></li>
            </ol>
        </td>
        <td rowspan="2">
            <ol>
                <li>Check if libibv_extend.so is installed.<br><code>ll /path/to/ibv_extend/output/lib</code><br>If there is output, libibv_extend is installed, done.<br>If no output, continue.</li>
                <li>Install libibv_extend, refer to <a href="#installation">Installation</a>.</li>
            </ol>
        </td>
    </tr>
    <tr>
        <td>Ubuntu/Debian</td>
        <td>
            <ol>
                <li>Check if libibverbs.so is installed.<br><code>ldconfig -p | grep libibverbs</code><br>If there is output, libibverbs.so is installed, done.<br>If no output, continue.</li>
                <li>Install libibverbs.<br><code>sudo apt update</code><br><code>sudo apt install libibverbs1 ibverbs-utils rdma-core</code></li>
            </ol>
        </td>
    </tr>
</table>

**Configure Environment Variables**

Upper-layer applications can configure **IBV_EXTEND_DRIVERS** environment variable before running to let ibv_extend component find and load specified driver extension libraries (such as 1825, Yunmai, and other network card extension drivers). Multiple drivers are separated by <strong>:</strong> or <strong>;</strong>.

Example: `export IBV_EXTEND_DRIVERS="/path1/my_ext_driver1.so:/path2/my_ext_driver2.so"`

**Configuration File**

1. Create extension driver configuration file directory.

    ```bash
    mkdir -p /etc/libibverbs_extend.d/
    ```

2. Enter extension driver configuration file directory.

    ```bash
    cd /etc/libibverbs_extend.d/
    ```

3. Create extension driver configuration file. Configuration file content format: driver <driver\_name\>.

    Assume driver extension library is libmy_ext_driver.so, use the following command to generate configuration file.

    ```bash
    echo "driver my_ext_driver" > my_ext_driver.conf
    ```

# API Reference

## Common Interfaces

### ibv_extend_get_version

**Function Prototype**

`const char *ibv_extend_get_version(uint32_t *major, uint32_t *minor, uint32_t *patch)`

**Function**

Get ibv_extend dynamic library version number.

**Parameters**

- major: Major version number. Output parameter. Can be null, null means not getting, interface does not allocate additional memory.
- minor: Minor version number. Output parameter. Can be null, null means not getting, interface does not allocate additional memory.
- patch: Patch version number. Output parameter. Can be null, null means not getting, interface does not allocate additional memory.

**Return Value**

Version string pointer (static string, no need to release).

**Usage Example**

```c
// Call function to get version information
version_str = ibv_extend_get_version(&major, &minor, &patch);
// Print version information
printf("Version string: %s\n", version_str);
printf("Version number: %u.%u.%u\n", major, minor, patch);
```

### ibv_extend_check_version

**Function Prototype**

`int ibv_extend_check_version(uint32_t driver_major, uint32_t driver_minor, uint32_t driver_patch)`

**Function**

Check version compatibility. This function checks at runtime whether the header file version used during driver and upper-layer application compilation is compatible with current library version. We recommend calling this function for version check during driver and upper-layer application initialization.

Compatibility rules:

1. Major version number completely matches, then compatible.
2. Software minor version number <= library minor version number, then compatible.
3. If minor version numbers are the same, and software patch version number <= library patch version number, then compatible.

**Parameters**

- driver_major: Major version number, recommend passing IBV_EXTEND_VERSION_MAJOR.
- driver_minor: Minor version number, recommend passing IBV_EXTEND_VERSION_MINOR.
- driver_patch: Patch version number, recommend passing IBV_EXTEND_VERSION_PATCH.

**Return Value**

- 0: Compatible.
- -1: Incompatible.

**Usage Example**

```c
// Call function to check version compatibility
ret = ibv_extend_check_version(IBV_EXTEND_VERSION_MAJOR, IBV_EXTEND_VERSION_MINOR, IBV_EXTEND_VERSION_PATCH);
// Check return value
if (ret == 0) {
    printf("Version compatible, can use normally\n");
    // Continue subsequent initialization operations
} else {
    printf("Version incompatible, driver compilation version does not match current library version\n");
    // Refuse to run
    return -1;
}
```

## North-bound Interfaces (Context Management)

### ibv_open_extend

**Function Prototype**

`struct ibv_context_extend *ibv_open_extend(struct ibv_context *context)`

**Function**

Call driver registered interface to create extension context. Must call ibv_open_extend to create extension context before calling other interfaces.

**Parameters**

context: Standard ibv context, created using ibv_open_device, non-null.

**Return Value**

- Extension context pointer: Extension context creation successful.
- NULL: Extension context creation failed.

**Usage Example**

```c
// Get device list
int num_devices = 0;
struct ibv_device **dev_list = ibv_get_device_list(&num_devices);
if (!dev_list) {
    fprintf(stderr, "Failed to get device list\n");
    return -1;
}

// Check if there are available devices
if (num_devices <= 0) {
    fprintf(stderr, "No device found\n");
    ibv_free_device_list(dev_list);
    return -1;
}

// Select first device
struct ibv_device *device = dev_list[0];

// Open device
struct ibv_context *ctx = ibv_open_device(device);
if (!ctx) {
    fprintf(stderr, "Failed to open device\n");
    ibv_free_device_list(dev_list);
    return -1;
}

// Open extension context
struct ibv_context_extend *ext_ctx = ibv_open_extend(ctx);
if (!ext_ctx) {
    fprintf(stderr, "Failed to open extend context\n");
    ibv_close_device(ctx);
    ibv_free_device_list(dev_list);
    return -1;
}
```

### ibv_close_extend

**Function Prototype**

`int ibv_close_extend(struct ibv_context_extend *context)`

**Function**

Call driver registered interface to release extension context resources. Cannot use this extension context resource after ibv_close_extend call.

**Parameters**

context: Extension context, must be created using ibv_open_extend, non-null.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- ENOENT: Matching driver not found.
- Driver abnormal return value.

**Usage Example**

```c
int ret = ibv_close_extend(ext_ctx);
if (ret != 0) {
    fprintf(stderr, "Failed to close extend context: %d\n", ret);
}
```

## North-bound Interfaces (NPU Direct Async)

### ibv\_query\_device\_extend

**Function Prototype**

`int ibv_query_device_extend(struct ibv_context_extend *context, struct ibv_device_attr_extend *ext_dev_attr)`

**Function**

Query device extension capabilities.

**Parameters**

- context: Extension context, must be created using ibv_open_extend, non-null.
- ext_dev_attr: Returned device extension attributes, no additional memory allocated internally, non-null.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- Driver abnormal return value.

**Usage Example**

```c
int ret = ibv_query_device_extend(ext_ctx, &ext_dev_attr);
if (ret != 0) {
    fprintf(stderr, "Failed to query extend context: %d\n", ret);
}
if (ext_dev_attr.ext_cap & IBV_EXTEND_DEV_NDA) {
/* Device supports NDA */
}
```

### ibv\_create\_qp\_extend

**Function Prototype**

`struct ibv_qp_extend *ibv_create_qp_extend(struct ibv_context_extend *context, struct ibv_qp_init_attr_extend *qp_init_attr)`

**Function**

Call underlying driver's create_qp to create extended QP.

**Parameters**

- context: Extension context, must be created using ibv_open_extend, non-null.
- qp_init_attr: QP initialization configuration (includes qbuf DMA mode and resource callback functions), non-null.

**Return Value**

- QP extended structure pointer: Extended QP creation successful.
- NULL: Extended QP creation failed.

**Usage Example**

```c
struct ibv_qp_init_attr_extend qp_attr = {
    .pd = pd,
    .attr = {
    .cap = { .max_send_wr = 128, .max_recv_wr = 128 },
    .qp_type = IBV_QPT_RC,
    },
    .type = QU_BUF_DMA_MODE_DEFAULT,
    .ops = &my_extend_ops,
};
struct ibv_qp_extend *qp_ext = ibv_create_qp_extend(ext_ctx, &qp_attr);
```

### ibv\_create\_cq\_extend

**Function Prototype**

`struct ibv_cq_extend *ibv_create_cq_extend(struct ibv_context_extend *context, struct ibv_cq_init_attr_extend *cq_init_attr)`

**Function**

Call underlying driver's create_cq to create extended CQ.

**Parameters**

- context: Extension context, must be created using ibv_open_extend, non-null.
- cq_init_attr: CQ initialization configuration (includes qbuf DMA mode and resource callback functions), non-null.

**Return Value**

- CQ extended structure pointer: Extended CQ creation successful.
- NULL: Extended CQ creation failed.

**Usage Example**

```c
struct ibv_cq_init_attr_extend cq_attr = {
    .attr {
        .cqe = 128,
    },
    .type = QU_BUF_DMA_MODE_DEFAULT,
    .ops = &my_extend_ops,
};
struct ibv_cq_extend *cq_ext = ibv_create_cq_extend(ext_ctx, &cq_attr);
```

### ibv\_create\_srq\_extend

**Function Prototype**

`struct ibv_srq_extend *ibv_create_srq_extend(struct ibv_context_extend *context, struct ibv_srq_init_attr_extend *srq_init_attr)`

**Function**

Call underlying driver's create_srq to create extended SRQ.

**Parameters**

- context: Extension context, must be created using ibv_open_extend, non-null.
- srq_init_attr: SRQ initialization configuration (includes qbuf DMA mode and resource callback functions), non-null.

**Return Value**

- SRQ extended structure pointer: Extended SRQ creation successful.
- NULL: Extended SRQ creation failed.

### ibv\_destroy\_qp\_extend

**Function Prototype**

`int ibv_destroy_qp_extend(struct ibv_context_extend *context, struct ibv_qp_extend *qp_extend)`

**Function**

Call underlying driver's destroy_qp to destroy extended QP.

**Parameters**

- context: Extension context, must be created using ibv_open_extend, non-null.
- qp_extend: QP extended structure, must be created by ibv_create_qp_extend.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- Driver abnormal return value.

### ibv\_destroy\_cq\_extend

**Function Prototype**

`int ibv_destroy_cq_extend(struct ibv_context_extend *context, struct ibv_cq_extend *cq_extend)`

**Function**

Call underlying driver's destroy_cq to destroy extended CQ.

**Parameters**

- context: Extension context, must be created using ibv_open_extend, non-null.
- cq_extend: CQ extended structure, must be created by ibv_create_cq_extend.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- Driver abnormal return value.

### ibv\_destroy\_srq\_extend

**Function Prototype**

`int ibv_destroy_srq_extend(struct ibv_context_extend *context, struct ibv_srq_extend *srq_extend)`

**Function**

Call underlying driver's destroy_srq to destroy extended SRQ.

**Parameters**

- context: Extension context, must be created using ibv_open_extend, non-null.
- srq_extend: SRQ extended structure, must be created using ibv_create_srq_extend.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- Driver abnormal return value.

## North-bound Interfaces (Hyper RoCE)

Hyper RoCE interfaces are used to configure and query QP extended attributes for advanced RoCE features, including advanced RoCE feature type, load balancing mode, and selective retransmission parameters. Calling the following interfaces requires driver version >= IBV\_EXTEND\_DRIVER\_VERSION\_V2.

### ibv\_modify\_qp\_extend

**Function Prototype**

`int ibv_modify_qp_extend(struct ibv_context_extend *context, struct ibv_qp_attr_extend *attr, int attr_mask)`

**Function**

Modify QP extended attributes, supporting configuration of advanced RoCE features, load balancing mode, multi-path parameters, selective retransmission parameters, etc.

**Parameters**

- context: Extension context, must be created using ibv\_open\_extend, non-null.
- attr: QP extended attribute structure, containing attribute values to be modified, non-null.
- attr_mask: Attribute mask, specifying attributes to be modified, a bitwise combination of ibv\_qp\_attr\_extend\_mask enum type.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- EOPNOTSUPP: Driver version not supported (requires >= V2) or corresponding interface not implemented.

**Usage Example**

Each call to ibv\_modify\_qp\_extend should only modify one category of attributes (specified via attr\_mask). Different attribute categories need to be modified in separate calls. The example below demonstrates a typical Hyper RoCE initialization flow: first configure advanced RoCE features, then load balancing mode, and finally source UDP port.

```c
int init_hyper_roce(struct ibv_context_extend *ext_ctx, struct ibv_qp *qp)
{
    int rc;
    struct ibv_qp_attr_extend attr;

    /* 1. Configure advanced RoCE feature (type, version, SACK switch) */
    attr = (struct ibv_qp_attr_extend){0};
    attr.qp = qp;
    attr.feature.type = IBV_HYPER_FEAT_VEROCE;
    attr.feature.version = IBV_HYPER_FEAT_V2;
    attr.feature.sack_enable = 1;  /* Enable selective retransmission */

    rc = ibv_modify_qp_extend(ext_ctx, &attr, IBV_QP_ATTR_EXTEND_HYROCE_FEATURE);
    if (rc < 0) {
        fprintf(stderr, "Failed to modify QP with HYROCE feature, rc=%d\n", rc);
        return rc;
    }

    /* 2. Configure load balancing mode */
    attr = (struct ibv_qp_attr_extend){0};
    attr.qp = qp;
    attr.lb_mode = IBV_LB_MODE_MPATH;  /* Multi-Path mode */

    rc = ibv_modify_qp_extend(ext_ctx, &attr, IBV_QP_ATTR_EXTEND_LB_MODE);
    if (rc < 0) {
        fprintf(stderr, "Failed to modify QP with lb_mode, rc=%d\n", rc);
        return rc;
    }

    /* 3. Configure source UDP port */
    attr = (struct ibv_qp_attr_extend){0};
    attr.qp = qp;
    attr.udp_src_port = 12345;

    rc = ibv_modify_qp_extend(ext_ctx, &attr, IBV_QP_ATTR_EXTEND_UDP_SRC_PORT);
    if (rc < 0) {
        fprintf(stderr, "Failed to modify QP with UDP src port, rc=%d\n", rc);
        return rc;
    }

    return 0;
}
```

### ibv\_query\_qp\_extend

**Function Prototype**

`int ibv_query_qp_extend(struct ibv_context_extend *context, struct ibv_qp_attr_extend *attr, int attr_mask)`

**Function**

Query QP extended attributes, supporting query of advanced RoCE features, load balancing mode, multi-path parameters, selective retransmission parameters, etc.

**Parameters**

- context: Extension context, must be created using ibv\_open\_extend, non-null.
- attr: QP extended attribute structure, used to return query results. qp field must be set to target QP beforehand, non-null.
- attr_mask: Attribute mask, specifying attributes to be queried, a bitwise combination of ibv\_qp\_attr\_extend\_mask enum type.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- EOPNOTSUPP: Driver version not supported (requires >= V2) or corresponding interface not implemented.

**Usage Example**

Each call to ibv\_query\_qp\_extend should only query one category of attributes (specified via attr\_mask). Different attribute categories need to be queried in separate calls. Before querying, zero out the attr structure and set the qp field.

```c
int query_hyper_roce_info(struct ibv_context_extend *ext_ctx, struct ibv_qp *qp)
{
    int rc;
    struct ibv_qp_attr_extend attr;

    /* 1. Query advanced RoCE feature */
    attr = (struct ibv_qp_attr_extend){0};
    attr.qp = qp;

    rc = ibv_query_qp_extend(ext_ctx, &attr, IBV_QP_ATTR_EXTEND_HYROCE_FEATURE);
    if (rc < 0) {
        fprintf(stderr, "Failed to query QP HYROCE feature, rc=%d\n", rc);
        return rc;
    }
    printf("RoCE feature type=%d, version=%d, sack_enable=%d\n",
           attr.feature.type, attr.feature.version, attr.feature.sack_enable);

    /* 2. Query load balancing mode */
    attr = (struct ibv_qp_attr_extend){0};
    attr.qp = qp;

    rc = ibv_query_qp_extend(ext_ctx, &attr, IBV_QP_ATTR_EXTEND_LB_MODE);
    if (rc < 0) {
        fprintf(stderr, "Failed to query QP lb_mode, rc=%d\n", rc);
        return rc;
    }
    printf("LB mode=%d\n", attr.lb_mode);

    return 0;
}

```

## Northbound Interface (Hyper RoCE Negotiation)

The Hyper RoCE negotiation interface supports advanced RoCE features for NIC negotiation. Usage:

1) The NIC configures its supported advanced RoCE features through a tool.
The application retrieves the NIC's supported advanced RoCE features via the ibv_query_qp_supported_hyroce_feature interface, for sending to the peer NIC.
2) The application performs advanced RoCE negotiation with the NIC via the ibv_nego_qp_hyroce_feature interface, and the NIC returns the negotiated advanced RoCE features.
3) The application configures and enables the advanced RoCE features on the NIC via ibv_modify_qp_extend using the features returned from negotiation.
Calling these interfaces requires driver version >= IBV_EXTEND_DRIVER_VERSION_V3.

### ibv\_query\_qp\_supported\_hyroce\_feature

**Function Prototype**

```c
int ibv_query_qp_supported_hyroce_feature(struct ibv_context_extend *context,
                                          struct ibv_qp *qp,
                                          uint32_t sl,
                                          uint32_t tc,
                                          struct ibv_hyroce_feature *feature);
```

**Function**

Query NIC-supported RoCE feature capabilities.

**Parameters**

- context: context: Extended context, must be created via ibv\_open\_extend, non-null.
- qp: Verbs QP structure.
- sl: Service Level.
- tc: Traffic Class.
- feature: Advanced RoCE feature.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- EOPNOTSUPP: Driver version not supported (requires >= V3) or corresponding interface not implemented.

**Usage Example**

ibv\_query\_qp\_supported\_hyroce\_feature must be called before the QP enters RTR state.

```c
int query_qp_supported_hyroce_feature(struct ibv_context_extend *ext_ctx, struct ibv_qp *qp)
{
    int rc;
    int sl = 3;
    int tc = 10;
    struct ibv_hyroce_feature feature = {0};

    rc = ibv_query_qp_supported_hyroce_feature(ext_ctx, qp, sl, tc, &feature);
    if (rc < 0) {
        fprintf(stderr, "Failed to query QP Supported HYROCE feature, rc=%d\n", rc);
        return rc;
    }
    printf("RoCE feature type=%d, version=%d, sack_enable=%d\n",
           feature.type, feature.version, feature.sack_enable);

    return 0;
}
```

### ibv\_nego\_qp\_hyroce\_feature

**Function Prototype**

```c
int ibv_nego_qp_hyroce_feature(struct ibv_context_extend *context,
                               struct ibv_qp *qp,
                               const struct ibv_hyroce_feature *input,
                               struct ibv_hyroce_feature *output,
                               uint32_t *need_more_nego)
```

**Function**

Negotiates RoCE features for QP.

**Parameters**

- context: Extended context, must be created via ibv\_open\_extend non-null.
- qp: Verbs QP structure.
- input: Input RoCE feature.
- output: RoCE feature after NIC negotiation.
- need_more_nego: Whether further negotiation is needed.

**Return Value**

- 0: Success.
- EINVAL: Invalid parameter.
- EOPNOTSUPP: Driver version not supported (requires >= V3) or interface not implemented.

**Usage Example**

ibv\_nego\_qp\_hyroce\_feature is called before the QP reaches RTR state.

```c
int nego_qp_hyroce_feature(struct ibv_context_extend *ext_ctx, struct ibv_qp *qp)
{
    int rc;
    uint32_t need_more_nego = 0;
    struct ibv_hyroce_feature input = {
        .type = IBV_HYPER_FEAT_HYPER_ROCE,
        .version = IBV_HYPER_FEAT_V2,
    };

    struct ibv_hyroce_feature output = {0};

    rc = ibv_nego_qp_hyroce_feature(ext_ctx, qp, &input, &output, &need_more_nego);
    if (rc < 0) {
        fprintf(stderr, "Failed to nego QP HYROCE feature, rc=%d\n", rc);
        return rc;
    }
    if (need_more_nego != 0) {
        printf("Should nego QP HYROCE feature again");
    } else {
        printf("RoCE feature type=%d, version=%d, sack_enable=%d\n",
                output.type, output.version, output.sack_enable);
    }

    return 0;
}
```

## South-bound Interfaces (Driver Call)

### verbs_register_driver_extend

**Function Prototype**

`void verbsregister_driver_extend(const struct verbs_device_extend_ops *ops)` or `PROVIDER_EXTEND_DRIVER(drv)`

**Function**

Register driver to extend_driver_list linked list. Can use encapsulated PROVIDER_EXTEND_DRIVER macro.

**Parameters**

- ops: Device extension operation structure pointer, non-null, internally does not allocate additional address, must be global static variable.
- drv: Must be struct verbs_device_extend_ops type.

**Return Value**

None.

**Usage Example**

```c
static const struct verbs_device_extend_ops my_driver_ops = {
    .name = "my_driver",    // Must be consistent with standard driver
    .alloc_context = my_alloc_context,
    .free_context = my_free_context,
};
PROVIDER_EXTEND_DRIVER(my_driver_ops);  // Use macro for automatic registration
```

# Key Structure Description

## Enum Type Definition

### queue_buf_dma_mode

During extended QP/CQ/SRQ resource creation, for these resources, driver needs to establish host address and device address mapping so that device can directly drive network card resources. Currently supports two mapping modes.

| Variable | Value | Description |
| ---- | ---- | ---- |
| QU_BUF_DMA_MODE_DEFAULT | 0 | PCIe mapping mode, this mode supports general hardware form, network card and NPU are both PCIe devices, usually use peerMem mode mapping. |
| QU_BUF_DMA_MODE_INDEP_UB | 1 | UB mapping mode, network card and NPU are PCIe devices, but data path uses independent UB bus addressing, usually use UbShmem mode mapping. |

### doorbell_map_mode

During extended QP/CQ/SRQ resource creation, driver will allocate doorbell address, and needs to map host-side allocated doorbell address to NPU card side. Mapping logic (db_mmap) is implemented by upper layer. Upper layer needs to perform mapping based on mapping type provided by driver.

| Variable | Value | Description |
| ---- | ---- | ---- |
| DB_MAP_MODE_HOST_VA | 0 | Based on Host virtual address mapping, usually third-party network card resource mapping method. |
| DB_MAP_MODE_UB_RES | 1 | Use UB device resource descriptor for mapping, 1825 network card used mapping method. |

### memcpy_direction

When driver uses upper-layer registered memcpy_s to copy memory, need to specify copy direction to allow driver read/write access to device memory.

| Variable | Value | Description |
| ---- | ---- | ---- |
| MEMCPY_DIR_HOST_TO_HOST | 0 | memcpy_s parameter, copy within Host. |
| MEMCPY_DIR_HOST_TO_DEVICE | 1 | memcpy_s parameter, memory copy from Host to Device (NPU). |
| MEMCPY_DIR_DEVICE_TO_HOST | 2 | memcpy_s parameter, memory copy from Device (NPU) to Host. |
| MEMCPY_DIR_DEVICE_TO_DEVICE | 3 | memcpy_s parameter, memory copy within Device (NPU) or between Device (NPU). |

### ibv_qp_init_cap

When creating extended QP, upper-layer communication library can specify additional extension configuration, optional.

| Variable | Value | Description |
| ---- | ---- | ---- |
| QP_ENABLE_DIRECT_WQE | 1 | If network card supports direct_wqe capability, by configuring this field, can enable network card direct_wqe capability when calling ibv_create_qp_extend interface to create sq_info. |

### ibv_extend_device_cap

Device supported extension capabilities, returned by extension query interface. Driver queries whether current device supports NDR/NDA.

| Variable | Value | Description |
| ---- | ---- | ---- |
| IBV_EXTEND_NDR | 1 | Device supports NDR (NPUDirect Rdma) capability. |
| IBV_EXTEND_NDA | 2 | Device supports NDA (NPUDirect Async) capability. |

### IBV\_EXTEND\_DRIVER\_VERSION (Driver Operations Interface Version)

The version number for the driver-implemented ops set, indicating the range of feature interfaces the driver supports, ensuring compatibility between upper-layer library and underlying driver. Each version corresponds to a fixed set of function pointers. Version number must be incremented when new interfaces are added.

| Variable | Value | Description |
| ---- | ---- | ---- |
| IBV\_EXTEND\_DRIVER\_VERSION\_UNUSED | 0 | Version not set, not allowed. |
| IBV\_EXTEND\_DRIVER\_VERSION\_V1 | 1 | Version 1: Supports NPU Direct Async basic interfaces (create\_qp, create\_cq, create\_srq, destroy\_qp, destroy\_cq, destroy\_srq, query\_device). |
| IBV\_EXTEND\_DRIVER\_VERSION\_V2 | 2 | Version 2: All V1 interfaces + Hyper RoCE interfaces (modify\_qp, query\_qp). |

Version compatibility rules:

1. Drivers must set the correct version number during registration and initialize all function pointers supported by that version.
2. The upper-layer library checks driver version before calling interfaces; returns \-EOPNOTSUPP on version mismatch.
3. Old drivers can run on newer extension libraries, but can only use interfaces supported by the old driver.
4. New drivers cannot run on older extension libraries; the extension library needs to be upgraded.

### ibv\_hyroce\_feature\_type

Advanced RoCE feature type enum, used to specify the advanced RoCE protocol type used by QP.

| Variable | Value | Description |
| ---- | ---- | ---- |
| IBV\_HYPER\_FEAT\_RoCEv2 | 0 | Standard RoCEv2 protocol. |
| IBV\_HYPER\_FEAT\_VEROCE | 1 | RoCE for VelcEngine. |
| IBV\_HYPER\_FEAT\_HYPER_ROCE | 2 | Hyper RoCE. |

### ibv\_hyroce\_feature\_version

Advanced RoCE feature version enum, used to specify the advanced RoCE protocol version used by QP.

| Variable | Value | Description |
| ---- | ---- | ---- |
| IBV\_HYPER\_FEAT\_V0 | 0 | Not used. |
| IBV\_HYPER\_FEAT\_V1 | 1 | Version P1. |
| IBV\_HYPER\_FEAT\_V2 | 2 | Version P2. |
| IBV\_HYPER\_FEAT\_V3 | 3 | Version P3. |

### ibv\_lb\_mode

Load balancing mode enum, used to specify QP load balancing strategy.

| Variable | Value | Description |
| ---- | ---- | ---- |
| IBV\_LB\_MODE\_DEFAULT | 0 | Network card default load balancing mode. |
| IBV\_LB\_MODE\_MPATH | 1 | Multi-Path mode. |
| IBV\_LB\_MODE\_AR | 2 | Adaptive-Routing mode. |

### ibv\_qp\_attr\_extend\_mask

QP attribute extension mask enum, used to specify which attributes need to be configured or queried by ibv\_modify\_qp\_extend and ibv\_query\_qp\_extend.

| Variable | Value | Description |
| ---- | ---- | ---- |
| IBV\_QP\_ATTR\_EXTEND\_UDP\_SRC\_PORT | 1 << 0 | Source UDP port number. |
| IBV\_QP\_ATTR\_EXTEND\_HYROCE\_FEATURE | 1 << 1 | Advanced RoCE feature. |
| IBV\_QP\_ATTR\_EXTEND\_LB\_MODE | 1 << 2 | Load balancing mode. |
| IBV\_QP\_ATTR\_EXTEND\_MPATH\_CONFIG | 1 << 3 | Multi-Path configuration. |
| IBV\_QP\_ATTR\_EXTEND\_AR\_CONFIG | 1 << 4 | Adaptive-Routing configuration. |
| IBV\_QP\_ATTR\_EXTEND\_SACK\_CONFIG | 1 << 5 | Selective Ack configuration. |

## Core Data Structures

### doorbell_map_desc

doorbell_map descriptor is used for driver-side mapping of hardware doorbell address. After driver allocates doorbell, maps it to NPU through db_map callback passed by upper layer for NPU direct-drive.

| Variable | Type | Description |
| ---- | ---- | ---- |
| hva | uint64_t | doorbell virtual address on host side, PCIe network card uses, address must be valid. |
| ub_res | struct | UB mapping method uses, uses UB resource descriptor, address must be valid. See structure description below. |
| size | uint64_t | doorbell mapping length. |
| type | uint32_t | doorbell mapping type (doorbell_map_mode type). |
| resv | uint32_t | Reserved. |

```c
struct {
    uint64_t guid_l;   /* GUID low 64 bits */
    uint64_t guid_h;   /* GUID high 64 bits */
    struct {
        uint64_t resource_id : 4; /* Corresponds to res_id in GUID */
        uint64_t offset : 32;  /* Offset within a page id of a certain GUID */
        uint64_t resv : 28;   /* Reserved */
    } bits;
} ub_res;
```

### ibv_extend_ops

This series of functions is used for memory allocation and mapping during QP/CQ/SRQ and other resource creation. Upper-layer application implements, for underlying driver callback use.

| Function Prototype | Function Description | Input Parameter Description | Return Value Description |
| ---- | ---- | ---- | ---- |
| `void *(*alloc)(size_t size)` | Used by underlying driver to request memory resources, can be NPU memory resources or host memory resources. | size: Allocation length, unit bytes, greater than 0. | Allocated memory pointer, returns null on allocation failure. |
| `void (*free)(void *ptr)` | Used by underlying driver for memory release. Memory must be address allocated by alloc callback function. | ptr: Memory pointer allocated using alloc interface. | No return value. |
| `void (*memset_s)(void *dst, int value, size_t count)` | Used by underlying driver for memory initialization operation. Memory must be address allocated by alloc callback function. | <li>dst: Memory pointer allocated using alloc interface, memset_s does not perform boundary check on memory area pointed by pointer ptr, so need to ensure memory area pointed by ptr is large enough when using, avoid out-of-bounds access.</li><li>value: Usually an int type value, but actually only uses low 8 bits of this value. This means values outside range 0 to 255 may produce undefined behavior.</li><li>count: Number of bytes to set, usually calculated through sizeof() or other means. count cannot be greater than size requested during alloc.</li> | No return value. |
| `int (*memcpy_s)(void *dst, size_t dst_max_size, void *src, size_t size, uint32_t direct)` | Used by underlying driver for memory copy operation. Memory must be address allocated by alloc callback function. | <li>dst: Copy destination address, can be host-side allocated address or alloc allocated address.</li><li>dst_max_size: For copy safety, need to pass maximum length of dst destination address.</li><li>src: Copy source address, can be host-side allocated address or alloc allocated address.</li><li>size: Actual copy length in src, unit bytes, cannot exceed src allocated length.</li><li>direct: Copy direction, type is enum memcpy_direction.</li> | Returns 0 on success, returns negative error code on failure. |
| `void *(*db_mmap)(struct doorbell_map_desc *desc)` | Used for doorbell mapping logic, usually implements logic of mapping host doorbell address to NPU memory.<br>Implementation constraint: Must support repeated mapping of same desc, that is, multiple mappings of same desc address must return same mapping address. | desc: doorbell resource mapping descriptor. | Mapped address pointer, returns null on failure. |
| `int (*db_unmap)(void *ptr, struct doorbell_map_desc *desc)` | Used for unmapping logic. | <li>ptr: Address mapped by calling db_mmap interface.</li><li>desc: Input desc during db_mmap.</li> | Returns 0 on success, returns negative error code on failure. |

### ibv\_context\_extend\_ops

South-bound interface, implemented by device driver, called by ibv\_extend module. The structure contains a version field indicating the driver version, and function pointers for each feature module.

| Member | Description |
| ---- | ---- |
| `int version` | Driver operations interface version number, type is enum IBV\_EXTEND\_DRIVER\_VERSION. Driver must set the correct version number, used for version compatibility check. |
| `struct ibv_qp_extend *(*create_qp)(...)` | Extended QP creation interface, V1 version supported, driver implemented, null means not supported. |
| `struct ibv_cq_extend *(*create_cq)(...)` | Extended CQ creation interface, V1 version supported, driver implemented, null means not supported. |
| `struct ibv_srq_extend *(*create_srq)(...)` | Extended SRQ creation interface, V1 version supported, driver implemented, null means not supported. |
| `int (*destroy_qp)(struct ibv_qp_extend *qp_extend)` | Extended QP destruction interface, V1 version supported, driver implemented, null means not supported. |
| `int (*destroy_cq)(struct ibv_cq_extend *cq_extend)` | Extended CQ destruction interface, V1 version supported, driver implemented, null means not supported. |
| `int (*destroy_srq)(struct ibv_srq_extend *srq_extend)` | Extended SRQ destruction interface, V1 version supported, driver implemented, null means not supported. |
| `int (*query_device)(struct ibv_context *context, struct ibv_device_attr_extend *ext_dev_attr)` | Query device extension attributes interface, V1 version supported, driver implemented, null means not supported. |
| `int (*modify_qp)(struct ibv_context *context, struct ibv_qp_attr_extend *attr, int attr_mask)` | QP extended attribute modification interface, V2 version new addition, driver implemented, null means not supported. |
| `int (*query_qp)(struct ibv_context *context, struct ibv_qp_attr_extend *attr, int attr_mask)` | QP extended attribute query interface, V2 version new addition, driver implemented, null means not supported. |
| `int (*query_qp_supported_hyroce_feature)(struct ibv_context *context, struct ibv_qp *qp, uint32_t sl, uint32_t tc, struct ibv_hyroce_feature *feature);` | Query supported hyper RoCE feature , driver implemented, null means not supported. |
| `int (*nego_qp_hyroce_feature)(struct ibv_context *context, struct ibv_qp *qp, const struct ibv_hyroce_feature *input, struct ibv_hyroce_feature *output, uint32_t *need_more_nego);` | Negotiate hyper RoCE feature , driver implemented, null means not supported. |


### verbs\_device\_extend\_ops

Device-level extension operation interface, implemented by device driver. The name field must match the standard driver, used for matching the standard driver during extension driver registration.

| Member | Description |
| ---- | ---- |
| `const char *name` | Used to match original driver ibv_context, must be consistent with the name defined in verbs_device_ops by the driver vendor, cannot be null. |
| `struct ibv_context_extend *(*alloc_context)(struct ibv_context *context)` | Used to allocate extension context, driver implemented, null means not supported. |
| `void (*free_context)(struct ibv_context_extend *context)` | Used to release extension context, driver implemented, null means not supported. |

### ibv\_hyroce\_feature

Advanced RoCE feature configuration structure, used in ibv\_modify\_qp\_extend and ibv\_query\_qp\_extend operations to configure or query QP's advanced RoCE features.

| Variable | Type | Description |
| ---- | ---- | ---- |
| type | uint8\_t | Advanced RoCE type, value range: enum ibv\_hyroce\_feature\_type. |
| version | uint8\_t | Advanced RoCE version, value range: enum ibv\_hyroce\_feature\_version. |
| sack\_enable | uint8\_t | Selective retransmission switch, 0-off, 1-on. |
| resv | uint8\_t[] | Reserved field, for future feature extension. |

### ibv\_mpath\_config

Multi-Path configuration parameter structure, used to configure QP multi-path load balancing parameters.

| Variable | Type | Description |
| ---- | ---- | ---- |
| flowlet\_pkg\_num | uint32\_t | Number of packets per sub-flow (flowlet), used for flow splitting. |
| path\_num | uint32\_t | Number of multi-paths. |
| interval | uint32\_t | UDP port number increment interval, used to distinguish different paths. |
| path\_rr\_enable | uint32\_t | Whether path supports round-robin (RR) network port selection, 0-not supported, 1-supported. |
| resv | uint32\_t[] | Reserved field, for future extension. |

### ibv\_ar\_config

Adaptive Routing configuration structure, used to configure QP adaptive routing parameters.

| Variable | Type | Description |
| ---- | ---- | ---- |
| port\_rr\_enable | uint32\_t | Whether to enable network port side round-robin (RR) per-packet function, 0-off, 1-on. |
| resv | uint32\_t[] | Reserved field, for future extension. |

### ibv\_sack\_config

Selective Acknowledgment (SACK) configuration structure, used to configure QP selective retransmission parameters.

| Variable | Type | Description |
| ---- | ---- | ---- |
| srp\_range | uint32\_t | TX (send) direction maximum retransmission packet count. |
| oor\_range | uint32\_t | RX (receive) direction out-of-order retransmission window packet count. |
| resv | uint32\_t[] | Reserved field, for future extension. |

### ibv\_qp\_attr\_extend

QP extended attribute structure, used for ibv\_modify\_qp\_extend and ibv\_query\_qp\_extend operations. When using, specify which attribute fields need to be configured or queried through attr\_mask.

| Variable | Type | Description |
| ---- | ---- | ---- |
| qp | struct ibv\_qp * | QP handle, pointing to the QP object to be modified or queried. |
| udp\_src\_port | uint32\_t | Source UDP port number. |
| feature | struct ibv\_hyroce\_feature | Advanced RoCE feature. |
| lb\_mode | uint32\_t | Load balancing mode, value range: enum ibv\_lb\_mode. |
| mpath | struct ibv\_mpath\_config | Multi-Path configuration. |
| ar | struct ibv\_ar\_config | Adaptive Routing configuration. |
| sack | struct ibv\_sack\_config | Selective Acknowledgment (SACK) configuration. |
| resv | uint32\_t[] | Reserved field, for future extension. |
| lag_port | uint32\_t | Bond mode QP binding network port. |

# FAQ

## Error when calling ibv_open_extend interface to initialize extension library

**Problem Description**

Upper-layer communication component calls ibv_open_extend interface returns null pointer. After configuring **IBV_EXTEND_SHOW_WARNINGS** environment variable, error as follows:

```text
ibv_extend: Warning: no available ops for open extend context
```

**Possible Cause**

ibv_extend extension library cannot find corresponding network card extension driver or dlopen corresponding network card extension driver reports error.

**Solution**

1. Refer to [Usage](#usage), let ibv_extend find corresponding network card extension driver location.
2. If using dlopen method to load libibv_extend.so, need to export symbols to global symbol table through RTLD_GLOBAL. Usage example as follows.

    ```c
    void *handle = dlopen("libibv_extend.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "dlopen libibv_extend.so error: %s\n", dlerror());
        return;
    }
    ```

3. If still reports error, contact corresponding network card extension driver vendor to confirm whether extension driver package is correct.
