/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SVM_DEFINE_H__
#define SVM_DEFINE_H__

#include "ascend_hal_define.h"

#define BYTES_PER_KB 1024ul

#define DEVMM_MAX_NUMA_NUM_OF_PER_DEV 32U

#define DAVINCI_SVM_SUB_MODULE_NAME "SVM"
#define DAVINCI_SVM_AGENT_SUB_MODULE_NAME "SVM_AGENT"

#define DV_ADVISE_DDR 0x0001
#define DV_ADVISE_HBM 0x0002
#define DV_ADVISE_HUGEPAGE 0x0004
#define DV_ADVISE_POPULATE 0x0008
#define DV_ADVISE_P2P_HBM 0x0010
#define DV_ADVISE_P2P_DDR 0x0020
#define DV_ADVISE_LOCK_DEV 0x0080
#define DV_ADVISE_CONTINUTY 0x0100
#define DV_ADVISE_HOST 0x0200
#define DV_ADVISE_TS_DDR 0x0400
#define DV_ADVISE_PERSISTENT 0x0800
#define DV_ADVISE_READONLY 0x1000
#define DV_ADVISE_DEV_READONLY 0x2000
#define DV_ADVISE_NOCACHE 0x4000
#define DV_ADVISE_GIANTPAGE 0x8000
#define DV_ADVISE_HOST_UVA 0x10000
#define DV_ADVISE_SNAPSHOT_OPTIONAL 0x20000
#define DV_ADVISE_SNAPSHOT_REQUIRED 0x40000

#define DV_ADVISE_MODULE_ID_BIT 24
#define DV_ADVISE_MODULE_ID_MASK 0xff

#define AGENT_ADVISE_MEM_GET 0x0001
#define AGENT_ADVISE_MEM_PUT 0x0002

#define MAX_CONTINUTY_PHYS_SIZE 0x400000ULL /* 4M */

#ifdef CFG_SOC_PLATFORM_CLOUD_V2
/*
 * In 910B sriov/mdev:
 * devid = pf_id * MAX_VF_NUM(16) + (vfid - 1) + 100
 * pf:     devid 0   ~ devid 63
 * vf:     devid 100 ~ devid 1123
 * rsv:    devid 64  ~ devid 99
 *
 * device devid:
 * pf:     devid 0   ~ devid 31
 * vf:     devid 32  ~ devid 63
 */
#define DEVMM_MAX_VF_NUM 16
#define DEVMM_MAX_DEVICE_NUM 1124
#define DEVMM_MAX_AGENTMM_DEVICE_NUM 64
#define DEVMM_MAX_PHY_DEVICE_NUM 64
#define DEVMM_INVALID_DEVICE_PHYID DEVMM_MAX_DEVICE_NUM
#define SVM_DEVICE_SIDE_AGENT_NUM DEVMM_MAX_DEVICE_NUM
#define SVM_HOST_SIDE_AGENT_NUM 1
#define SVM_MAX_AGENT_NUM DEVMM_MAX_DEVICE_NUM
#else
#define DEVMM_MAX_VF_NUM 32
#define DEVMM_MAX_DEVICE_NUM 64
#define DEVMM_MAX_AGENTMM_DEVICE_NUM 4
#define DEVMM_MAX_PHY_DEVICE_NUM DEVMM_MAX_DEVICE_NUM
#define DEVMM_INVALID_DEVICE_PHYID 0xff
#define SVM_DEVICE_SIDE_AGENT_NUM DEVMM_MAX_DEVICE_NUM
#define SVM_HOST_SIDE_AGENT_NUM 2 /* 64 + 1 + 1, host id is 65 */
#define SVM_MAX_AGENT_NUM (SVM_DEVICE_SIDE_AGENT_NUM + SVM_HOST_SIDE_AGENT_NUM)
#endif

#define DEVMM_MAX_LOGIC_DEVICE_NUM 66ULL /* 64 + 1 + 1, host id is 65 */
#define SVM_HOST_AGENT_ID 64
/* 64 dvpp + 1 read + 1 dev_readonly + 1 normal + 1 pcie through + 1 uvm */
#ifndef UVM_OPEN
#define DEVMM_NORMAL_MAX_VMA_NUM (DEVMM_MAX_PHY_DEVICE_NUM + 6)
#else
#define DEVMM_NORMAL_MAX_VMA_NUM (DEVMM_MAX_PHY_DEVICE_NUM + 5)
#endif
#define DEVMM_MAX_VMA_NUM (DEVMM_NORMAL_MAX_VMA_NUM * 2)

#ifndef UVM_OPEN
/* vma0: dvpp mem, vma1 read  vma2: dev readonly, vma3 normal mem, vma4 soma mem, vma5 uvm mem */
#define DEVMM_NORMAL_UNSPLIT_MAX_VMA_NUM 6
#else
#define DEVMM_NORMAL_UNSPLIT_MAX_VMA_NUM 5
#endif
#define DEVMM_UNSPLIT_MAX_VMA_NUM (DEVMM_NORMAL_UNSPLIT_MAX_VMA_NUM * 2)
#define DEVMM_SVM_DEV_NAME "devmm_svm"
#define DEVMM_SVM_AGENT_DEV_NAME "devmm_svm_agent"
#define DEVMM_SVM_DEV_PATH "/dev/"

#define DEVMM_DEFAULT_HOST_UVA_START 0x70000000000UL /* 7TB */
#define DEVMM_HCCS_HOST_UVA_START 0x80000000000UL    /* 8TB */
#ifndef CFG_FEATURE_ENABLE_ASAN
/* orther addr not test, dev mmap fail because program segments loading random */
#define DEVMM_SVM_MEM_START 0x100000000000ULL /* 16TB */
uint64_t devmm_get_host_uva_start(void);
#define DEVMM_HOST_PIN_START devmm_get_host_uva_start()
#else
#define DEVMM_SVM_MEM_START 0x210000000000ULL /* device asan 0x100000000000ULL mmap fail */
#define DEVMM_HOST_PIN_START 0x140000000000UL /* 20TB */
#endif
#define DEVMM_SOMA_OFFSET_RANGE 42                                     /* 4TB */
#define DEVMM_SOMA_OFFSET (1ULL << DEVMM_SOMA_OFFSET_RANGE)            /* 4TB */
#define DEVMM_SOMA_MEM_START (DEVMM_SVM_MEM_START - DEVMM_SOMA_OFFSET) /* 12TB */
#define DEVMM_SOMA_SIZE_RANGE 40                                       /* 1TB */
#define DEVMM_SOMA_MEM_SIZE (2ULL << DEVMM_SOMA_SIZE_RANGE)            /* 2TB */
#define DEVMM_SOMA_PAGE_SIZE_RANGE 20                                  /* 1MB */
#define DEVMM_SOMA_PAGE_SIZE (2ULL << DEVMM_SOMA_PAGE_SIZE_RANGE)      /* 2MB */
#define DEVMM_HOST_PIN_SIZE 0x10000000000UL                            /* 1TB */
#define DEVMM_HOST_PIN_END (DEVMM_HOST_PIN_START + DEVMM_HOST_PIN_SIZE)
#define DEVMM_DEV_MAPPED_RANGE 37                                                  /* 128G */
#define DEVMM_MAPPEDSZ_PER_DEV (1ULL << DEVMM_DEV_MAPPED_RANGE)                    /* 128G */
#define DEVMM_MAX_MAPPED_RANGE (DEVMM_MAPPEDSZ_PER_DEV * DEVMM_MAX_PHY_DEVICE_NUM) /* 8T */
#define DEVMM_MAX_ALLOC_PAGE_NUM ((1ULL << 29) - 1)                                /* the max page num of ioctl alloc */

#define DEVMM_HEAP_RANGE 30
#define DEVMM_HEAP_SIZE (1UL << DEVMM_HEAP_RANGE) /* 1G */
#define DEVMM_MAX_HEAP_NUM (DEVMM_MAX_MAPPED_RANGE >> DEVMM_HEAP_RANGE)
#define DEVMM_MAX_HOST_PIN_HEAP_NUM (DEVMM_HOST_PIN_SIZE >> DEVMM_HEAP_RANGE) /* 1024 */
#define DEVMM_SVM_MEM_SIZE DEVMM_MAX_MAPPED_RANGE

#define DEVMM_PROC_USER_VA_MAX_SIZE 0x1000000000000ULL /* 256T */
#define DEVMM_MAX_DYN_ALLOC_BASE (DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE)
#define DEVMM_MAX_DYN_ALLOC_SIZE (DEVMM_PROC_USER_VA_MAX_SIZE - DEVMM_MAX_DYN_ALLOC_BASE)
#define DEVMM_MAX_DYN_ALLOC_HEAP_NUM (DEVMM_MAX_DYN_ALLOC_SIZE >> DEVMM_HEAP_RANGE)
#define DEVMM_TOTAL_MAX_HEAP_NUM (DEVMM_MAX_HEAP_NUM + DEVMM_MAX_DYN_ALLOC_HEAP_NUM)
#ifndef UVM_OPEN
#define UVM_HOST_ID 64
#define UVM_MAX_DEVICE_ID 64
#define DEVMM_HOST_MAX_VMA_NUM 2
#define DEVMM_UVM_MEM_START 0x90000000000UL
#define DEVMM_UVM_MEM_SIZE 0x30000000000UL /* 3T */
#define DEVMM_UVM_PAGE_RANGE 21            /* 2M */
#define DEVMM_UVM_PAGE_NUM (DEVMM_UVM_MEM_SIZE >> DEVMM_UVM_PAGE_RANGE)
#define DEVMM_UVM_PAGE_SIZE 2097152ULL
#define UVM_PAGE_MASK (~(DEVMM_UVM_PAGE_SIZE - 1))
#define UVM_PAGE_SHIFT 21
#define MAX_CONT_NUM 1
#define DEVMM_UVM_SYNC_ID 0
#define DEVMM_UVM_TIMEOUT 5000 /* 5s */
#endif
/* dvpp engine addressing range split in two, one for drv, one for os share pool */
#define DEVMM_TWICE_DVPP_HEAP_SIZE 2
#define DEVMM_MAX_HEAP_MEM_FOR_DVPP_4G 0x100000000Ul  /* 4G */
#define DEVMM_MAX_HEAP_MEM_FOR_DVPP_16G 0x400000000Ul /* 16G */
#define DEVMM_DVPP_HEAP_MAX_SIZE DEVMM_MAX_HEAP_MEM_FOR_DVPP_16G
#define DEVMM_DVPP_HEAP_RESERVATION_SIZE 0x800000000Ul /* 2 * max */
#define DEVMM_DVPP_HEAP_TOTAL_SIZE 0x20000000000Ul     /* 2 * max * DEVMM_MAX_DEVICE_NUM */

#define DEVMM_READ_ONLY_HEAP_SIZE 0x100000000Ul        /* 4G */
#define DEVMM_READ_ONLY_HEAP_TOTAL_SIZE 0x4000000000Ul /* 64 * 4G */
#define DEVMM_READ_ONLY_ADDR_START (DEVMM_SVM_MEM_START + DEVMM_DVPP_HEAP_TOTAL_SIZE)
#define DEVMM_READ_ONLY_ADDR_END (DEVMM_READ_ONLY_ADDR_START + DEVMM_READ_ONLY_HEAP_TOTAL_SIZE - 1)

#define DEVMM_DEV_READ_ONLY_HEAP_SIZE 0x200000000Ul        /* 8G */
#define DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE 0x8000000000Ul /* 64 * 8G */
#define DEVMM_DEV_READ_ONLY_ADDR_START \
    (DEVMM_SVM_MEM_START + DEVMM_DVPP_HEAP_TOTAL_SIZE + DEVMM_READ_ONLY_HEAP_TOTAL_SIZE)
#define DEVMM_DEV_READ_ONLY_ADDR_END (DEVMM_DEV_READ_ONLY_ADDR_START + DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE - 1)

#define DEVMM_NON_RESERVATION_HEAP_ADDR_START                                             \
    (DEVMM_SVM_MEM_START + DEVMM_DVPP_HEAP_TOTAL_SIZE + DEVMM_READ_ONLY_HEAP_TOTAL_SIZE + \
     DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE)

#define DEVMM_DVPP_ADDR_START DEVMM_SVM_MEM_START
#define DEVMM_RO_ADDR_START (DEVMM_DVPP_ADDR_START + DEVMM_DVPP_HEAP_TOTAL_SIZE)
#define DEVMM_DEV_RO_ADDR_START (DEVMM_RO_ADDR_START + DEVMM_READ_ONLY_HEAP_TOTAL_SIZE)
#define DEVMM_NORMAL_ADDR_START (DEVMM_DEV_RO_ADDR_START + DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE)

#define DEVMM_NORMAL_ADDR_SIZE                                                           \
    (DEVMM_SVM_MEM_SIZE - DEVMM_DVPP_HEAP_TOTAL_SIZE - DEVMM_READ_ONLY_HEAP_TOTAL_SIZE - \
     DEVMM_DEV_READ_ONLY_HEAP_TOTAL_SIZE)

#define DEVMM_REG_MAP_OFFSET 0x200000000 /* 8G, 2G per dev * 4 max pf dev */
#define DEVMM_REG_MAP_START (DEVMM_DCACHE_ADDR_START - DEVMM_REG_MAP_OFFSET)

#define DEVMM_DCACHE_OFFSET 0x1000000000ULL /* 64G  */
#define DEVMM_DCACHE_ADDR_START (DEVMM_SVM_MEM_START - DEVMM_DCACHE_OFFSET)

#define HCCS_AGENT_EXCLUSIVE_SPACE_MAX_ADDR 0X40000000 /* low 1G */

#define DEVMM_INVALID_ADDR 0UL
#define DEVMM_INVALID_ADDR2 1UL /* for ioctl err */

#define DEVMM_TRUE 1UL
#define DEVMM_FALSE 0UL

#define DEVMM_CONVERT_64M_SIZE 0x4000000UL
#define DEVMM_CONVERT_128M_SIZE 0x8000000UL
#define DEVMM_CONVERT_DMA_DEPTH 0x8000UL
#define DEVMM_CONVERT2D_HEIGHT_MAX 0x500000UL /* 5M */

#define DEVMM_ASYNC_SUBMIT_FINISH_VALUE 0x3a3a3a3aUL
#define DEVMM_ASYNC_CPY_FINISH_VALUE 0x5a5a5a5aUL
#define DEVMM_ASYNC_CPY_ERR_VALUE (-18) /*  EXDEV Cross-device link */

#define DEVMM_SHARE_MEM_MAX_PID_CNT 65535 /* > 64 process * 384 server node * 2 die */

#define SVM_INVALID_SDID (UINT_MAX)

/*
 * device:
 * setup device:
 * UNINITED->h2d set POLLING->polled wateup thread set POLLED->forked thread set dev pid & PRE_INITING
 * ->mmap set INITING->ioctl set INITED
 * user drvMemInitSvmDevice:
 * UNINITED->ioctl set pid & PRE_INITING->mmap set INITING->ioctl set INITED
 *
 * host:
 * UNINITED->mmap set pid& INITING->ioctl set INITED
 */
#define DEVMM_SVM_UNINITED_FLAG 0
#define DEVMM_SVM_PRE_INITING_FLAG 3
#define DEVMM_SVM_INITING_FLAG 4
#define DEVMM_SVM_RELEASE_FLAG 5
#define DEVMM_SVM_PRE_INITED_FLAG 0xAABBCCDD
#define DEVMM_SVM_INITED_FLAG 0xFFAACCEE
#define DEVMM_SVM_UNMAP_FLAG 0xAADDCC

#define DEVMM_SVM_INVALID_PID (-1)
#define DEVMM_SVM_INVALID_INDEX (-1)

#define DEVMM_GIANT_PAGE_SIZE 0x40000000ULL
#define DEVMM_GIANT_TO_HUGE_PAGE_NUM 512ULL

enum devmm_endpoint_type {
    DEVMM_END_HOST = 0x0,
    DEVMM_END_DEVICE,
    DEVMM_END_NUM
};

enum devmm_side_type {
    DEVMM_SIDE_MASTER = 0,
    DEVMM_SIDE_DEVICE_AGENT = 1,
    DEVMM_SIDE_HOST_AGENT = 2,
    DEVMM_SIDE_MAX
};

enum devmm_mem_type {
    DEVMM_HBM_MEM = 0x0,
    DEVMM_DDR_MEM,
    DEVMM_P2P_HBM_MEM,
    DEVMM_P2P_DDR_MEM,
    DEVMM_TS_DDR_MEM,
#ifndef UVM_OPEN
    DEVMM_UVM_MEM,
#endif
    DEVMM_SOMA_MEM,
    DEVMM_HOST_PIN_MEM,
    DEVMM_MEM_TYPE_MAX,
};

enum devmm_page_type {
    DEVMM_NORMAL_PAGE_TYPE = 0x0,
    DEVMM_HUGE_PAGE_TYPE,
    DEVMM_GIANT_PAGE_TYPE,
    DEVMM_PAGE_TYPE_MAX
};

enum devmm_copy_direction {
    DEVMM_COPY_HOST_TO_HOST,
    DEVMM_COPY_HOST_TO_DEVICE,
    DEVMM_COPY_DEVICE_TO_HOST,
    DEVMM_COPY_DEVICE_TO_DEVICE,
    DEVMM_COPY_INVILED_DIRECTION
};

#define DEVMM_HEAP_ENABLE (0ul)
#define DEVMM_HEAP_DISABLE (1ul)

#define DEVMM_HEAP_PINNED_HOST (0xEFEF0001UL)
#define DEVMM_HEAP_HUGE_PAGE (0xEFEF0002UL)
#define DEVMM_HEAP_CHUNK_PAGE (0xEFEF0003UL) // page_size is exchanged svm page_size

#define DEVMM_HEAP_IDLE (0xEFEFABCDUL)
#define DEVMM_HEAP_OVERSIZED (0xEFEFAAAAUL)

enum devmm_heap_sub_type {
    SUB_SVM_TYPE = 0x0,           /* user mode page is same as kernel page, huge or chunk. the same as MEM_SVM_VAL */
    SUB_DEVICE_TYPE = 0x1,        /* user mode page is same as kernel page, just huge. the same as MEM_DEV_VAL */
    SUB_HOST_TYPE = 0x2,          /* user mode page is same as kernel page just chunk. the same as MEM_HOST_VAL */
    SUB_DVPP_TYPE = 0x3,          /* kernel page is huge, user mode page is chunk. the same as MEM_DVPP_VAL */
    SUB_READ_ONLY_TYPE = 0x4,     /* kernel page is huge, user mode page is chunk. MEM_DEV_VAL */
    SUB_RESERVE_TYPE = 0X5,       /* For halMemAddressReserve */
    SUB_DEV_READ_ONLY_TYPE = 0x6, /* kernel page is huge, user mode page is chunk. MEM_DEV_VAL */
#ifndef UVM_OPEN
    SUB_UVM_TYPE = 0x7,
#endif
    SUB_SOMA_TYPE = 0x8,
    SUB_MAX_TYPE
};

#define DEVMM_COPY_LEN_CNT0 (0)
#define DEVMM_COPY_LEN_CNT1 (1)
#define DEVMM_COPY_LEN_CNT2 (2)
#define DEVMM_COPY_LEN_CNT3 (3)
#define DEVMM_COPY_LEN_CNT4 (4)
#define DEVMM_COPY_LEN_CNT32 (32)

/* process normal exit and annormal exit flags */
#define DEVMM_SVM_NORMAL_EXITED_FLAG (0xEEEEEEEE)
#define DEVMM_SVM_ABNORMAL_EXITED_FLAG (0)

#define SVM_MEM_HANDLE_NORMAL_TYPE 0
#define SVM_MEM_HANDLE_EXPORT_TYPE 1
#define SVM_MEM_HANDLE_IMPORT_TYPE 2
#define SVM_MEM_HANDLE_SHARE_TYPE 3

static inline bool devmm_is_in_mapped_range(unsigned long long va)
{
    return ((va >= DEVMM_SVM_MEM_START) && (va < DEVMM_SVM_MEM_START + DEVMM_SVM_MEM_SIZE));
}

static inline bool devmm_is_in_host_pin_range(unsigned long long va)
{
    return ((va >= DEVMM_HOST_PIN_START) && (va < DEVMM_HOST_PIN_END));
}
static inline bool devmm_va_is_in_soma_range(unsigned long long va)
{
    return ((va >= DEVMM_SOMA_MEM_START) && (va < DEVMM_SOMA_MEM_START + DEVMM_SOMA_MEM_SIZE));
}
#ifndef UVM_OPEN
static inline bool devmm_va_is_in_uvm_range(unsigned long long va)
{
    return ((va >= DEVMM_UVM_MEM_START) && (va < DEVMM_UVM_MEM_START + DEVMM_UVM_MEM_SIZE));
}
#endif
static inline bool devmm_va_is_in_svm_range(unsigned long long va)
{
    return devmm_is_in_mapped_range(va) || devmm_is_in_host_pin_range(va);
}
#ifndef UVM_OPEN
static inline bool devmm_vaddr_and_size_is_in_uvm_range(uint64_t va, uint64_t size)
{
    return ((va >= DEVMM_UVM_MEM_START) && (va < DEVMM_UVM_MEM_START + DEVMM_UVM_MEM_SIZE) &&
            (size <= DEVMM_UVM_MEM_START + DEVMM_UVM_MEM_SIZE - va));
}
#endif

/*=============================== mem stats start =============================*/
#define SVM_MAX_MODULE_ID MAX_MODULE_ID

#if defined(DRV_HOST) || defined(DEVMM_UT)
#define MEM_STATS_DEVICE_CNT (64 + 1 + 1) /* device 64 + 1, host id is 65 */
#define MEM_STATS_MAX_MEM_VAL 3
#define MEM_STATS_MAX_PAGE_TYPE 2
#define MEM_STATS_MAX_PHY_MEMTYPE 5
#else
#define MEM_STATS_DEVICE_CNT 4
#define MEM_STATS_MAX_MEM_VAL 1
#define MEM_STATS_MAX_PAGE_TYPE 2
#define MEM_STATS_MAX_PHY_MEMTYPE 3
#endif

struct svm_mem_stats {
    unsigned long long current_alloced_size[SVM_MAX_MODULE_ID];
    unsigned long long alloced_peak_size[SVM_MAX_MODULE_ID];
    unsigned long long mapped_size;
    unsigned long long alloc_cnt[SVM_MAX_MODULE_ID];
    unsigned long long free_cnt[SVM_MAX_MODULE_ID];
};

struct svm_mem_stats_type {
    uint32_t mem_val;
    uint32_t page_type;
    uint32_t phy_memtype;
};

#define MEM_STATS_MNG_SIZE \
    (sizeof(struct svm_mem_stats) * MEM_STATS_MAX_MEM_VAL * MEM_STATS_MAX_PAGE_TYPE * MEM_STATS_MAX_PHY_MEMTYPE)

static inline void svm_mem_stats_type_pack(struct svm_mem_stats_type *type, uint32_t mem_val, uint32_t page_type,
                                           uint32_t phy_memtype)
{
    type->mem_val = mem_val;
    type->page_type = page_type;
    type->phy_memtype = phy_memtype;
}

#if defined(DRV_HOST) || defined(DEVMM_UT)
static inline const char *svm_get_mem_type_str(uint32_t mem_val, uint32_t page_type, uint32_t phy_memtype)
{
    if (mem_val == MEM_SVM_VAL) {
        return "SVM_MEM";
    } else if (mem_val == MEM_HOST_VAL) {
        return "HOST_MEM";
    } else {
        static const char *mem_type_str[MEM_STATS_MAX_PAGE_TYPE][MEM_STATS_MAX_PHY_MEMTYPE] = {
            {"MEM_DEV_SMALL_HBM", "MEM_DEV_SMALL_DDR", "MEM_DEV_SMALL_P2P_HBM", "MEM_DEV_SMALL_P2P_DDR",
             "MEM_DEV_SMALL_TS_DDR"},
            {"MEM_DEV_HUGE_HBM", "MEM_DEV_HUGE_DDR", "MEM_DEV_HUGE_P2P_HBM", "MEM_DEV_HUGE_P2P_DDR",
             "MEM_DEV_HUGE_TS_DDR"}};

        return mem_type_str[page_type][phy_memtype];
    }
}
#else
static inline const char *svm_get_mem_type_str(uint32_t mem_val, uint32_t page_type, uint32_t phy_memtype)
{
    static const char *mem_type_str[MEM_STATS_MAX_PAGE_TYPE][MEM_STATS_MAX_PHY_MEMTYPE] = {
        {"MEM_DEV_SMALL_HBM", "MEM_DEV_SMALL_DDR", "MEM_DEV_SMALL_TS_DDR"},
        {"MEM_DEV_HUGE_HBM", "MEM_DEV_HUGE_DDR", "MEM_DEV_HUGE_TS_DDR"}};

    return mem_type_str[page_type][phy_memtype];
}
#endif

/*=============================== mem stats end =============================*/

#endif
