/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#define _GNU_SOURCE
#include "securec.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t handle;
    uint8_t flag;
    uint16_t pod_id;
    uint8_t slot_id;
    uint8_t mesh_type;
    uint32_t super_pod_id;
    uint16_t served_index;
    uint8_t type_name_idx;
} ub_info_t;
#pragma pack(pop)

static const char *find_dmi_table_path(void)
{
    static const char *candidates[] = {"/sys/firmware/dmi/tables/DMITable", "/sys/firmware/dmi/tables/DMI", NULL};
    for (int i = 0; candidates[i]; i++) {
        if (access(candidates[i], R_OK) == 0) {
            return candidates[i];
        }
    }
    return NULL;
}

static const char *get_smbios_string(const uint8_t *tbl, size_t max, uint8_t idx)
{
    if (idx == 0) {
        return "";
    }
    const uint8_t *p = tbl, *end = tbl + max;
    uint8_t cur = 1;
    while (p < end) {
        if (cur == idx) {
            return (const char *)p;
        }
        if (*p == '\0') {
            cur++;
            p++;
            if (p < end && *p == '\0') {
                break;
            }
        } else {
            p++;
        }
    }
    return "<N/A>";
}

static size_t str_tbl_len(const uint8_t *d, size_t max)
{
    const uint8_t *p = d, *end = d + max;
    while (p + 1 < end) {
        if (!p[0] && !p[1]) {
            return (size_t)(p + 2 - d);
        }
        p++;
    }
    return (size_t)(p + 1 - d);
}

static void print_table_header(void)
{
    printf("┌────────┬──────────────┬─────────┬──────────────────┬──────────────────────────┐\n");
    printf("│ Offset │ Name         │ Length  │ Raw Value        │ Parsed Value             │\n");
    printf("├────────┼──────────────┼─────────┼──────────────────┼──────────────────────────┤\n");
}

static void print_table_footer(void)
{
    printf("└────────┴──────────────┴─────────┴──────────────────┴──────────────────────────┘\n");
}

/* Print a standard table row with raw and parsed values */
static void print_row(const char *off, const char *name, const char *len, const char *raw_fmt, const char *parse_fmt,
                      ...)
{
    char _r[32], _p[64];
    va_list ap;

    va_start(ap, parse_fmt);
    if (vsnprintf_s(_r, sizeof(_r), sizeof(_r) - 1, raw_fmt, ap) < 0) {
        va_end(ap);
        return;
    }
    va_end(ap);

    va_start(ap, parse_fmt);
    if (vsnprintf_s(_p, sizeof(_p), sizeof(_p) - 1, parse_fmt, ap) < 0) {
        va_end(ap);
        return;
    }
    va_end(ap);

    printf("│ %-6s │ %-12s │ %-7s │ %-16s │ %-24s │\n", off, name, len, _r, _p);
}

/* Format and print a single hex-value row with invalid-value handling */
static void print_hex_row(const char *off, const char *name, const char *typ, const char *r_fmt, unsigned long inv_val,
                          const char *inv_str, unsigned long val)
{
    char r[32], p[64];

    if (snprintf_s(r, sizeof(r), sizeof(r) - 1, r_fmt, val) < 0) {
        return;
    }
    if (snprintf_s(p, sizeof(p), sizeof(p) - 1, (val == inv_val) ? inv_str : "%lu", val) < 0) {
        return;
    }
    printf("│ %-6s │ %-12s │ %-7s │ %-16s │ %-24s │\n", off, name, typ, r, p);
}

static void print_flag_row(uint8_t flag)
{
    char r[32], p[64];
    int ret = snprintf_s(r, sizeof(r), sizeof(r) - 1, "0x%02X", flag);
    if (ret < 0) {
        return;
    }
    if (flag == 0x00) {
        ret = strncpy_s(p, sizeof(p), "OK (BMC online)", sizeof(p) - 1);
    } else if (flag == 0xFF) {
        ret = strncpy_s(p, sizeof(p), "FAILED (BMC off)", sizeof(p) - 1);
    } else {
        ret = snprintf_s(p, sizeof(p), sizeof(p) - 1, "Unknown (0x%02X)", flag);
    }
    if (ret < 0) {
        return;
    }
    printf("│ %-6s │ %-12s │ %-7s │ %-16s │ %-24s │\n", "04h", "Flag", "BYTE", r, p);
}

static void print_interpreted_rows(const ub_info_t *info, const uint8_t *stbl, size_t stlen)
{
    char r[32], p[64];
    int ret;

    print_flag_row(info->flag);

    print_hex_row("05h", "PodId", "WORD", "0x%04lX", 0xFFFFUL, "Invalid (0xFFFF)", (unsigned long)info->pod_id);
    print_hex_row("07h", "SlotId", "BYTE", "0x%02lX", 0xFFUL, "Invalid (0xFF)", (unsigned long)info->slot_id);

    /* Mesh_Type */
    ret = snprintf_s(r, sizeof(r), sizeof(r) - 1, "0x%02X", info->mesh_type);
    if (ret < 0) {
        return;
    }
    const char *ms = (info->mesh_type == 1) ? "1D" :
                     (info->mesh_type == 2) ? "2D" :
                     (info->mesh_type == 3) ? "3D" :
                                              "Unk";
    ret = snprintf_s(p, sizeof(p), sizeof(p) - 1, "%s (%u)", ms, info->mesh_type);
    if (ret < 0) {
        return;
    }
    printf("│ %-6s │ %-12s │ %-7s │ %-16s │ %-24s │\n", "08h", "Mesh_Type", "BYTE", r, p);

    print_hex_row("09h", "SuperPodId", "DWORD", "0x%08lX", 0xFFFFFFFFUL, "Invalid (0xFFFFFFFF)",
                  (unsigned long)info->super_pod_id);
    print_hex_row("0Dh", "ServedIndex", "WORD", "0x%04lX", 0xFFFFUL, "Invalid (0xFFFF)",
                  (unsigned long)info->served_index);

    /* TypeName */
    const char *tn = get_smbios_string(stbl, stlen, info->type_name_idx);
    ret = snprintf_s(r, sizeof(r), sizeof(r) - 1, "idx=%u", info->type_name_idx);
    if (ret < 0) {
        return;
    }
    ret = snprintf_s(p, sizeof(p), sizeof(p) - 1, "\"%s\"", tn);
    if (ret < 0) {
        return;
    }
    printf("│ %-6s │ %-12s │ %-7s │ %-16s │ %-24s │\n", "0Fh", "TypeName", "String", r, p);
}

static void print_reserved_row(const uint8_t *raw, size_t slen)
{
    size_t rlen = (slen > 0x10) ? slen - 0x10 : 0;
    char rhex[32] = "00 00 00 00 ...";
    int ret;
    if (rlen == 0) {
        ret = strncpy_s(rhex, sizeof(rhex), "(none)", sizeof(rhex) - 1);
        if (ret < 0) {
            return;
        }
    } else {
        size_t show = (rlen > 8) ? 8 : rlen;
        int pos = 0;
        for (size_t i = 0; i < show && pos < 24; i++) {
            ret = snprintf_s(rhex + pos, sizeof(rhex) - pos, sizeof(rhex) - pos - 1, "%02X ", raw[0x10 + i]);
            if (ret < 0) {
                return;
            }
            pos += ret;
        }
        if (rlen > 8) {
            rhex[pos - 1] = '.';
            rhex[pos] = '.';
            rhex[pos + 1] = '.';
            rhex[pos + 2] = '\0';
        }
    }
    char rlen_str[32];
    ret = snprintf_s(rlen_str, sizeof(rlen_str), sizeof(rlen_str) - 1, "%zu BYTE", rlen);
    if (ret < 0) {
        return;
    }
    printf("│ %-6s │ %-12s │ %-7s │ %-16s │ %-24s │\n", "10h", "Reserved", rlen_str, "", rhex);
}

static void print_table(const ub_info_t *info, const uint8_t *raw, size_t slen, const uint8_t *stbl, size_t stlen)
{
    print_table_header();

    print_row("00h", "Type", "BYTE", "0x%02X", "%u", info->type);
    print_row("01h", "Length", "BYTE", "0x%02X", "%u bytes", info->length);
    print_row("02h", "Handle", "WORD", "0x%04X", "0x%04X", info->handle);

    print_interpreted_rows(info, stbl, stlen);
    print_reserved_row(raw, slen);

    print_table_footer();
}

static int read_dmi_table(int fd, uint8_t *tbl, size_t tsz)
{
    size_t rd = 0;
    while (rd < tsz) {
        ssize_t n = read(fd, tbl + rd, tsz - rd);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "read: errno=%d\n", errno);
            return -1;
        }
        if (n == 0) {
            break;
        }
        rd += (size_t)n;
    }
    return (int)rd;
}

static int scan_type131(const uint8_t *tbl, size_t tsz)
{
    size_t off = 0;
    int found = 0;
    while (off + 4 <= tsz) {
        const uint8_t *h = tbl + off;
        if (h[1] < 4 || off + h[1] > tsz) {
            break;
        }
        if (h[0] == 131) {
            if (h[1] < 0x10) {
                fprintf(stderr, "Warn: Type131 too short (%u)\n", h[1]);
                break;
            }
            const ub_info_t *info = (const ub_info_t *)(tbl + off);
            const uint8_t *st = tbl + off + h[1];
            size_t sl = str_tbl_len(st, tsz - off - h[1]);
            print_table(info, tbl + off, h[1], st, sl);
            found++;
        }
        off += h[1] + str_tbl_len(tbl + off + h[1], tsz - off - h[1]);
    }
    return found;
}

int main(void)
{
    const char *path = find_dmi_table_path();
    if (path == NULL) {
        fprintf(stderr, "Error: No DMI table found. Run as root.\n");
        return 1;
    }
    printf("DMI Table: %s\n\n", path);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "open: errno=%d\n", errno);
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        fprintf(stderr, "fstat: errno=%d\n", errno);
        close(fd);
        return 1;
    }
    size_t tsz = (size_t)st.st_size;
    uint8_t *tbl = (uint8_t *)malloc(tsz);
    if (tbl == NULL) {
        fprintf(stderr, "malloc failed\n");
        close(fd);
        return 1;
    }

    int rd = read_dmi_table(fd, tbl, tsz);
    if (rd < 0) {
        free(tbl);
        close(fd);
        return 1;
    }
    tsz = (size_t)rd;
    close(fd);

    int found = scan_type131(tbl, tsz);
    if (found == 0) {
        printf("SMBIOS Type 131 not found.\n");
    }
    free(tbl);
    return found ? 0 : 1;
}