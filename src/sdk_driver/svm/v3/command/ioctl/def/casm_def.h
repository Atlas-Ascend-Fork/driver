/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef CASM_DEF_H
#define CASM_DEF_H

enum casm_pg_type {
    CASM_PG_TYPE_INVALID = 0, /* Source page type is unavailable in cross-server IPC. */
    CASM_NPAGE,
    CASM_HPAGE,
    CASM_GPAGE,
    CASM_PG_TYPE_MAX
};

struct casm_src_prop {
    enum casm_pg_type casm_pg_type;
};

#endif
