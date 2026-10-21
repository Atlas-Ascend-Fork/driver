/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef CASM_IOCTL_H
#define CASM_IOCTL_H

#include "svm_pub.h"
#include "svm_addr_desc.h"
#include "casm_def.h"

#define SVM_CASM_TASK_OP_ADD 0U
#define SVM_CASM_TASK_OP_DEL 1U
#define SVM_CASM_TASK_OP_CHECK 2U
#define SVM_CASM_TASK_OP_MAX 3U

struct svm_casm_create_key_para {
    u32 task_type;                 /* input */
    u64 va;                        /* input */
    u64 size;                      /* input */
    struct casm_src_prop src_prop; /* input */
    u64 key;                       /* output */
    u64 rsv;                       /* reserve */
};

struct svm_casm_destroy_key_para {
    u64 key; /* input */
    u64 rsv; /* reserve */
};

struct svm_casm_op_task_para {
    u32 op;        /* input */
    u64 key;       /* input */
    u32 server_id; /* input */
    int tgid;      /* input */
    u64 rsv;       /* reserve */
};

struct svm_casm_get_src_va_para {
    u64 key;                       /* input */
    struct svm_global_va src_va;   /* output */
    struct casm_src_prop src_prop; /* output */
    u64 updated_va;                /* output */
    u64 ex_info;                   /* output */
    u64 rsv;                       /* reserve */
};

struct svm_casm_mem_pin_para {
    u64 va;   /* input */
    u64 size; /* input */
    u64 key;  /* input */
    u64 rsv;  /* reserve */
};

struct svm_casm_mem_unpin_para {
    u64 va;   /* input */
    u64 size; /* input */
    u64 rsv;  /* reserve */
};

struct svm_casm_cs_query_src_para {
    u64 key;                     /* input */
    struct svm_global_va src_va; /* output */
    int owner_pid;               /* output */
    u64 rsv;                     /* reserve */
};

struct svm_casm_cs_set_src_para {
    u64 key;                       /* input */
    struct svm_global_va src_va;   /* input */
    struct casm_src_prop src_prop; /* input */
    int owner_pid;                 /* input */
    u64 rsv;                       /* reserve */
};

struct svm_casm_cs_clr_src_para {
    u64 key; /* input */
    u64 rsv; /* reserve */
};

#endif
