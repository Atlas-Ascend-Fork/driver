/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _HDCDRV_MEM_COM_H_
#define _HDCDRV_MEM_COM_H_

#include "ka_memory_pub.h"
#include "ka_list_pub.h"
#include "ka_task_pub.h"
#ifdef CFG_FEATURE_VFIO
#include "vmng_kernel_interface.h"
#endif

#include "hdcdrv_cmd.h"
#include "hdcdrv_cmd_ioctl.h"
#include "hdcdrv_cmd_msg.h"

#define VHDC_VCTX_HASH_KEY_INVALID ((unsigned long long)-1)
#define VHDC_MEM_POOL_SG_FLAG 0xE8
#define HDCDRV_INVALID_VALUE (-1)

#ifdef CFG_FEATURE_SMALL_HUGE_POOL
#define HDCDRV_SEND_ALLOC_MEM_RETRY_TIME 1200
#else
#define HDCDRV_SEND_ALLOC_MEM_RETRY_TIME 600
#endif

struct hdcdrv_mem {
    void *buf;
};

/*
 * hdc memory pool.
 * size : memory block count
 * segment : memory block size, include the block head
 * head : Point to where the effective memory on the ring begins
 * tail : Point to the released ring position, when the memory is released, put it in this position
 */
struct hdcdrv_mem_pool {
    u32 valid;
    u32 dev_id;
    u32 size;
    u32 segment;
    u32 mask;
    u32 resv;
    u64 head;
    u64 tail;
    struct hdcdrv_mem *ring;
    ka_task_spinlock_t mem_lock;
    ka_list_head_t wait_list;
    void __ka_mm_iomem *reserve_mem_va;
    ka_dma_addr_t reserve_mem_dma_addr;
    size_t reserve_mem_size;
};

struct hdcdrv_mem_work {
    ka_delayed_work_t dwork;
    char *buf;
};

/*
 * hdc block memory head.
 * magic: magic number, used for check head
 * devid: which device belongs to
 * type : memory type, HDCDRV_MEM_POOL_TYPE_TX or HDCDRV_MEM_POOL_TYPE_RX
 * size : memory block size, include the block head
 * dma_addr : dma address
 * head_crc : crc32 from magic to dma_addr, used for check head
 * ref_count: the block reference count
 */
struct hdcdrv_mem_block_head {
    u32 magic;
    u32 devid;
    u32 type;
    u32 size;
    u32 offset; // dma addr seque offset
    void *dma_buf;
    ka_dma_addr_t dma_addr;
    ka_page_t *pool_page;
    u32 head_crc;
    u32 ref_count;
    ka_atomic_t status;
};

#define HDCDRV_MEM_CACHE_LINE 64
#define HDCDRV_MEM_BLOCK_MAGIC 0x48444342 /* HDCB */

#define HDCDRV_BLOCK_STATE_NORMAL 0
#define HDCDRV_BLOCK_STATE_OCCUPY 1

/* sizeof(struct hdcdrv_mem_block_head) must less than HDCDRV_MEM_CACHE_LINE */
#define HDCDRV_MEM_BLOCK_HEAD_SIZE 0
#define HDCDRV_BLOCK_CRC_LEN ka_offsetof(struct hdcdrv_mem_block_head, head_crc)

#define HDCDRV_BLOCK_HEAD(buf) (struct hdcdrv_mem_block_head *)((char *)(buf) - HDCDRV_MEM_BLOCK_HEAD_SIZE)
#define HDCDRV_BLOCK_DMA_HEAD(addr) (ka_dma_addr_t)((u64)(addr) - HDCDRV_MEM_BLOCK_HEAD_SIZE)

#define HDCDRV_BLOCK_BUFFER(head) (struct hdcdrv_mem_block_head *)((char *)(head) + HDCDRV_MEM_BLOCK_HEAD_SIZE)
#define HDCDRV_BLOCK_DMA_BUFFER(addr_head) (ka_dma_addr_t)((u64)(addr_head) + HDCDRV_MEM_BLOCK_HEAD_SIZE)

struct hdccom_mem_init {
    ka_device_t *dev;
    int pool_type;
    int dev_id;
    u32 segment;
    u32 num;
    /* reserve mem info */
    phys_addr_t reserve_mem_pa;
    size_t reserve_mem_size;
    void __ka_mm_iomem *reserve_mem_va;
    ka_dma_addr_t reserve_mem_dma_addr;
    u32 reserve_mem_offset;
};

struct hdccom_alloc_mem_para {
    int pool_type;
    int dev_id;
    int len;
    u32 fid;
    bool is_vm;
    ka_list_head_t *wait_head;
};

/* fast mem use define and struct in ctrl->device[devid] */
#define HDCDRV_SYNC_CHECK 1
#define HDCDRV_SYNC_NO_CHECK 0

#define HDCDRV_BUF_LEN 256

#define HDCDRV_LIST_MEM_NUM 1024

#define HDCDRV_RGISTER_MEM 2
#define HDCDRV_DMA_MEM 1
#define HDCDRV_NORMAL_MEM 0

#define HDCDRV_MAX_COST_TIME 100
#define HDCDRV_ALLOC_POOL_MAX_TIME 125
#define HDCDRV_INIT_TRANS_MAX_TIME 600
#define HDCDRV_THRESHOLD_COST_TIME 5000

#define HDCDRV_FRBTREE_FID_BEG 60
#define HDCDRV_FRBTREE_FID_MASK 0xFULL
#define HDCDRV_FRBTREE_PID_BEG 0
#define HDCDRV_FRBTREE_PID_MASK 0x0FFFFFFFULL
#define HDCDRV_FRBTREE_ADDR_BEG 28
#define HDCDRV_FRBTREE_ADDR_DEL 12
#define HDCDRV_FRBTREE_ADDR_MASK 0x00000FFFFFFFF000ULL
#define HDCDRV_HASH_VA_PIDADDR_MASK 0x0FFFFFFFFFFFFFFFULL
#define HDCDRV_HASH_VA_ADDR_MASK 0x0FFFFFFFF0000000ULL

/* tree search type */
#define HDCDRV_SEARCH_WITH_HASH 0U
#define HDCDRV_SEARCH_WITH_VA 1U
#define HDCDRV_SEARCH_NODE_CONFLICT 2U
#define HDCDRV_SEARCH_NODE_EXIST 3U

/* nodes relationship */
#define HDCDRV_NODES_SUBSET 0U
#define HDCDRV_NODES_INTER_SECTION 1U
#define HDCDRV_NODES_EMPTY_SET 2U

/* fast node ops type */
#define HDCDRV_FAST_NODE_INSERT 0U
#define HDCDRV_FAST_NODE_ERASE 1U
#define HDCDRV_FAST_NODE_SEARCH 2U

#define HDCDRV_QUICK_RELEASE_FLAG 1U
struct hdcdrv_node_status {
    u32 status;
    u32 hold_flag;
    u64 stamp;
};

struct hdcdrv_wait_mem_fin_msg {
    unsigned long long dataAddr;
    unsigned long long ctrlAddr;
    unsigned int dataLen;
    unsigned int ctrlLen;
    int status;
};

/* fifo size in elements (bytes) */
#define HDC_WAIT_MEM_FIFO_SIZE (128U * sizeof(struct hdcdrv_wait_mem_fin_msg))

struct hdcdrv_mem_f {
    void *buf;
#ifdef CFG_FEATURE_OVER_XCOM
    phys_addr_t phy_addr;
#endif
    ka_dma_addr_t addr;
    ka_page_t *page;
#ifdef CFG_FEATURE_VFIO
    ka_sg_table_t *dma_sgt;
#endif
    u32 len;
    u32 power;
    u32 type;
    u32 page_inner_offset; /* register memory va start offset within 4k */
};

struct hdcdrv_fast_mem {
    int phy_addr_num;
    u32 alloc_len;
    int mem_type;
    u32 page_type;
    int dma_map;
    int devid;
    u64 user_va;
    u64 hash_va;
    struct hdcdrv_mem_f *mem;

#ifdef CFG_FEATURE_HDC_REG_MEM
    u32 align_size;                 /* register memory align size, HUGE_PAGE->2M, NORMAL_PAGE->4k */
    u32 register_inner_page_offset; /* register memory va start offset within the corresponding PAGE(HUGE or Normal) */
#endif
#ifdef CFG_FEATURE_OVER_XCOM
    dma_addr_t sdma_addr;
    struct scatterlist *sg;
#endif
};

struct hdcdrv_mem_dfx_stat {
    u64 alloc_cnt;
    u64 alloc_size;
    u64 alloc_normal_size;
    u64 alloc_dma_size;
    u64 free_cnt;
    u64 free_size;
    ka_task_spinlock_t lock;
};

struct hdcdrv_dev_fmem {
    ka_rb_root_t rbtree;
    ka_rb_root_t rbtree_re;
    ka_task_spinlock_t rb_lock;
    struct hdcdrv_mem_dfx_stat mem_dfx_stat;
};

struct hdcdrv_mem_node_info {
    u64 hash_va;
    long long pid;
    int mem_type;
    u32 alloc_len;
    u64 user_va;
};

struct hdcdrv_mem_fd_list {
    struct hdcdrv_ctx_fmem *ctx_fmem;
    struct hdcdrv_ctx_fmem *async_ctx;
    struct hdcdrv_fast_node *f_node;
    ka_list_head_t list;
    struct hdcdrv_mem_node_info f_info;
};

struct hdcdrv_ctx_fmem {
    struct hdcdrv_mem_fd_list mlist;
    ka_task_spinlock_t mem_lock;
    u64 mem_count;
    int quick_flag;
};

struct hdcdrv_fast_node {
    ka_rb_node_t node;
    u64 hash_va;
    long long pid;
    u64 stamp;
    u32 max_cost;
    ka_atomic_t status;
    void *ctx;
    u32 unregister_flag; /* use to indicate if mem is unregistering */
    struct hdcdrv_mem_fd_list *mem_fd_node;
    struct hdcdrv_fast_mem fast_mem; // must be last one
};

struct hdcdrv_fast_addr_info {
#ifdef CFG_FEATURE_HDC_REG_MEM
    /* first page addr offset, eg: start cpy addr = va_addr + pagesize(4k) * page_start_Idx + page_start_offset */
    u32 page_start_idx;         /* first use page id, default:0 */
    u32 send_inner_page_offset; /* actually send va start offset within the corresponding PAGE(HUGE or Normal) */
#endif
#ifdef CFG_FEATURE_OVER_XCOM
    u32 send_offset;
#endif
    struct hdcdrv_fast_mem *f_mem;
};

struct hdcdrv_fast_page_info {
    u32 send_inner_page_offset;
    u32 page_start;
    u32 page_end;
};

struct hdcdrv_fast_mem_info {
    struct hdcdrv_fast_addr_info src_data;
    struct hdcdrv_fast_addr_info dst_data;
    struct hdcdrv_fast_addr_info src_ctrl;
    struct hdcdrv_fast_addr_info dst_ctrl;
};

struct hdcdrv_fast_node_msg_info {
    u32 dev_id;
    u32 process_stage; /* 0:register 1:send or recv 2:unregister */

    u64 pid;
    u32 fid;

    u32 len;
    u64 va_addr;

    u64 hash_val;
};

#define HDCDRV_SEARCH_NODE_REGISTER 0U
#define HDCDRV_SEARCH_NODE_SENDRECV 1U
#define HDCDRV_SEARCH_NODE_UNREGISTER 2U
#define HDCDRV_SEARCH_NODE_REMOTE 3U

struct hdcdrv_node_search_info {
    u32 search_type;   /* 0:search with hash_va 1:search with va */
    u32 process_stage; /* 0:register 1:send or recv 2:unregister, valid when search_type is 1*/
};

struct hdcdrv_node_tree_info {
    u32 fid;
    u32 rsv;
    u64 pid;
    ka_atomic_t refcnt;
    int idx; /* idx in array */
};

struct hdcdrv_node_local_tree {
    struct hdcdrv_node_tree_info tree_info;
    struct hdcdrv_dev_fmem fmem;
};

struct hdcdrv_node_re_tree {
    struct hdcdrv_node_tree_info tree_info;
    struct hdcdrv_dev_fmem devices[HDCDRV_SUPPORT_MAX_DEV];
};

struct hdcdrv_node_tree {
    /* local tree */
    struct hdcdrv_node_local_tree local_tree;
    /* remote tree */
    struct hdcdrv_node_re_tree re_tree;
};

struct hdcdrv_node_tree_ctrl {
    ka_rwlock_t lock;
    struct hdcdrv_node_tree node_tree[HDCDRV_SUPPORT_MAX_FID_PID];
};

struct hdcdrv_init_stamp {
    u64 wait_mutex_start;
    u64 wait_mutex_end;
    u64 chan_alloc_start;
    u64 chan_alloc_end;
    u64 chan_memset_end;
    u64 init_pool_start;
    u64 alloc_mem_pool0;
    u64 alloc_mem_pool1;
    u64 alloc_mem_pool2;
    u64 alloc_mem_pool3;
    u64 init_pool_end;
    u64 tasklet_start;
    u64 tasklet_end;
    u64 end;
};
struct hdcdrv_alloc_pool_stamp {
    u64 ring_alloc_start;
    u64 ring_alloc_end;
    u64 list_init_end;
    u64 dma_alloc_start;
    u64 dma_alloc_end;
    u64 head_alloc_end;
    u64 dma_alloc_max;
    u64 head_alloc_max;
    u64 dma_alloc_total;
    u64 head_alloc_total;
    u64 end;
};

struct hdcdrv_mem_list_entry {
    u32 devid;
    u64 hash_va;
    ka_list_head_t list;
};

struct hdcdrv_mem_list {
    u32 list_len;
    struct hdcdrv_mem_list_entry ka_list_entry;
    ka_task_spinlock_t lock;
};

int hdccom_alloc_mem(struct hdcdrv_mem_pool *pool, void **buf, ka_dma_addr_t *addr, u32 *offset);
int hdccom_free_mem(struct hdcdrv_mem_pool *pool, void *buf);
int hdccom_init_mem_pool(struct hdcdrv_mem_pool *pool, struct hdccom_mem_init *init_mem);

#ifdef CFG_FEATURE_VFIO
int hdccom_rx_comm_msg_para_check(u32 dev_id, u32 fid, const struct vmng_rx_msg_proc_info *proc_info);
int hdccom_rx_vpc_msg_para_check(u32 dev_id, u32 fid, const struct vmng_rx_msg_proc_info *proc_info);
int hdccom_rx_vpc_cmd_type_check(unsigned int cmd, const struct vmng_rx_msg_proc_info *proc_info);
int hdccom_rx_comm_msg_type_check(unsigned int cmd_min_len, const struct vmng_rx_msg_proc_info *proc_info);
void hdccom_fill_cmd_size_table(void);
#endif

int hdccom_free_mem_pool(struct hdcdrv_mem_pool *pool, ka_device_t *dev, u32 segment);
int hdcdrv_mem_block_head_check(void *buf);
void free_mem_pool_single(ka_device_t *dev, u32 segment, struct hdcdrv_mem_block_head *buf, ka_dma_addr_t addr);
long hdcdrv_get_page_size(struct hdcdrv_cmd_get_page_size *cmd);

ka_delayed_work_t *hdcdrv_get_recycle_mem(void);
ka_device_t *hdcdrv_get_pdev_dev(int dev_id);
struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_uni(void);
struct hdcdrv_node_tree_ctrl *hdcdrv_get_node_tree(void);
struct hdcdrv_node_tree_info *hdcdrv_get_node_tree_info(int idx, u32 rb_side);
struct hdcdrv_dev_fmem *hdcdrv_get_ctrl_arry_uni_ex(int idx, int devid, u32 side);
int hdcdrv_dma_unmap(struct hdcdrv_fast_mem *f_mem, u32 devid, int sync, int flag);
struct hdcdrv_dev_fmem *hdcdrv_get_dev_fmem_ex(int devid, u32 fid, u32 side);
struct hdcdrv_fast_node *hdcdrv_fast_node_search_from_new_tree(u32 rb_side, int timeout,
                                                               struct hdcdrv_fast_node_msg_info *node_info);
void hdcdrv_node_msg_info_fill(u32 pid, u32 fid, int len, u64 addr, u32 process_stage,
                               struct hdcdrv_fast_node_msg_info *msg);
void hdcdrv_fast_node_arry_tree_reset(ka_task_spinlock_t *lock, ka_rb_root_t *root,
                                      struct hdcdrv_node_tree_ctrl *ctrl_arry);
struct hdcdrv_fast_node *hdcdrv_fast_nodes_conflict(ka_task_spinlock_t *lock, ka_rb_root_t *root,
                                                    struct hdcdrv_fast_node *new_node);
struct hdcdrv_fast_node *hdcdrv_fast_node_arry_search(ka_task_spinlock_t *lock, ka_rb_root_t *root,
                                                      struct hdcdrv_fast_node_msg_info *node_info);
void hdcdrv_fast_register_recycle(const struct hdcdrv_cmd_register_mem *cmd, struct hdcdrv_fast_node *f_node);
long hdccom_fast_register_mem(struct hdcdrv_cmd_register_mem *cmd, struct hdcdrv_fast_node **f_node_ret);
ka_rb_root_t *hdcdrv_get_rbtree(struct hdcdrv_dev_fmem *dev_fmem, u32 side);
void hdcdrv_fast_mem_uninit(ka_task_spinlock_t *lock, ka_rb_root_t *root, int reset, int flag);
void hdcdrv_get_fast_mem(struct hdcdrv_dev_fmem *dev_fmem, int type, struct hdcdrv_fast_node_msg_info *node_msg,
                         struct hdcdrv_fast_addr_info *addr_info);
int hdcdrv_fast_node_insert_new_tree(int devid, u64 pid, u32 fid, u32 rb_side, struct hdcdrv_fast_node *new_node);
struct hdcdrv_fast_mem *hdcdrv_get_fast_mem_timeout(int dev_id, int type, int len, u64 hash_va, u64 user_va);
void hdcdrv_kvfree(void **addr, int level);

extern long hdcdrv_fast_dma_map(const struct hdcdrv_cmd_dma_map *cmd);
extern long hdcdrv_fast_dma_unmap(const struct hdcdrv_cmd_dma_unmap *cmd);
extern long hdcdrv_fast_dma_remap(const struct hdcdrv_cmd_dma_remap *cmd);
extern long hdcdrv_fast_unregister_mem(const void *ctx, struct hdcdrv_cmd_unregister_mem *cmd);
extern void hdcdrv_fast_mem_arry_uninit(void);
extern void hdcdrv_fast_node_erase_from_new_tree(u64 pid, u32 fid, int devid, u32 rb_side,
                                                 struct hdcdrv_fast_node *fast_node);
extern void hdcdrv_fast_node_free(const struct hdcdrv_fast_node *fast_node);
extern void hdcdrv_fast_free_phy_mem(struct hdcdrv_fast_mem *f_mem);

void *hdcdrv_kvmalloc(size_t size, int level);

void hdcdrv_fast_mem_continuity_check(u32 alloc_len, u32 addr_num, const int segment_mem_num[], u32 segment_num);
void hdcdrv_recycle_mem_work(ka_work_struct_t *p_work);
struct hdcdrv_fast_node *hdcdrv_fast_node_search_timeout(ka_task_spinlock_t *lock, ka_rb_root_t *root, u64 hash_va,
                                                         int timeout);
int hdcdrv_fast_node_insert(ka_task_spinlock_t *lock, ka_rb_root_t *root, struct hdcdrv_fast_node *fast_node,
                            u64 search_type);
void hdcdrv_fast_node_erase(ka_task_spinlock_t *lock, ka_rb_root_t *root, struct hdcdrv_fast_node *fast_node);
long hdccom_fast_alloc_mem(void *ctx, struct hdcdrv_cmd_alloc_mem *cmd, struct hdcdrv_fast_node **f_node_ret);
long hdcdrv_fast_free_mem(const void *ctx, struct hdcdrv_cmd_free_mem *cmd);

void hdcdrv_init_stamp_init(void);
void hdcdrv_alloc_pool_stamp_init(void);
void hdcdrv_set_time_stamp(u64 *stamp);
struct hdcdrv_init_stamp *hdcdrv_get_init_stamp_info(void);
void hdcdrv_init_stamp_record(void);
int hdcdrv_save_fast_mem_info(struct hdcdrv_fast_mem *fast_mem, u64 send_addr_va,
                              struct hdcdrv_fast_addr_info *addr_info);
void hdcdrv_fast_free_mem_node(struct hdcdrv_fast_mem *f_mem);
void hdcdrv_fast_unpin_mem_normal(struct hdcdrv_fast_mem *f_mem);
void hdcdrv_register_mem_scatter_list_uninit(struct hdcdrv_fast_mem *f_mem);
struct hdcdrv_fast_node *hdcdrv_fast_node_search(ka_task_spinlock_t *lock, ka_rb_root_t *root, u64 new_node_hash,
                                                 struct hdcdrv_node_status *node_status,
                                                 struct hdcdrv_node_search_info *node_search_info);
void hdcdrv_fast_free_huge_page_mem(struct hdcdrv_fast_mem *f_mem);
void hdcdrv_fill_fast_mem_info(struct hdcdrv_fast_mem *f_mem, u64 va, u32 len, u32 type);
int hdcdrv_dma_map(struct hdcdrv_fast_mem *f_mem, int devid, int flag);
void hdcdrv_huge_put_page(struct hdcdrv_fast_mem *f_mem);
int hdcdrv_pin_user_page(unsigned long start, int nr_pages, unsigned int gup_flags, ka_page_t **pages);
void hdcdrv_unpin_user_page(ka_page_t *page);
int hdcdrv_remote_mem_map(struct hdcdrv_fast_node *f_node, struct hdcdrv_ctrl_msg_sync_mem_info *msg);
void hdcdrv_remote_mem_unmap(struct hdcdrv_fast_node *f_node);
void hdcdrv_map_remote_pa(struct hdcdrv_fast_node *f_node, struct hdcdrv_ctrl_msg_sync_mem_info *msg);
int hdcdrv_register_mem_scatter_list_init(struct hdcdrv_fast_mem *f_mem);
#endif /* _HDCDRV_MEM_COM_H_ */
