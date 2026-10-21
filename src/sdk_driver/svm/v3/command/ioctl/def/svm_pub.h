/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef SVM_PUB_H
#define SVM_PUB_H

#ifndef u8
typedef unsigned char u8;
#endif

#ifndef u16
typedef unsigned short u16;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef u64
typedef unsigned long long u64;
#endif

#define svm_min(x, y) (((x) > (y)) ? (y) : (x))
#define svm_max(x, y) (((x) > (y)) ? (x) : (y))

#define SVM_IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)

#define SVM_BYTES_PER_KB 1024ULL
#define SVM_BYTES_PER_MB (1024ULL * SVM_BYTES_PER_KB)
#define SVM_BYTES_PER_GB (1024ULL * SVM_BYTES_PER_MB)
#define SVM_BYTES_PER_TB (1024ULL * SVM_BYTES_PER_GB)

#define SVM_UNUSED(expr) \
    do {                 \
        (void)(expr);    \
    } while (0)

#define SVM_MAX_DEV_AGENT_NUM 65U
#define SVM_MAX_AGENT_NUM SVM_MAX_DEV_AGENT_NUM
#define SVM_MAX_VF_NUM 32U
#define SVM_MAX_DEV_NUM (SVM_MAX_DEV_AGENT_NUM + 1U) /* 65 device + 1 host */
#define SVM_DEFAULT_HOST_DEVID SVM_MAX_AGENT_NUM

#define SVM_INVALID_SERVER_ID 0x3FFU
#define SVM_INVALID_DEVID SVM_MAX_DEV_NUM
#define SVM_INVALID_UDEVID 0xFFFFFFFF

#define SVM_ANY_TASK_ID (-2)

static inline u64 svm_align_up(u64 value, u64 align)
{
    if (align == 0) {
        return value;
    }

    if (value > (~0ULL - (align - 1ULL))) {
        return ~0ULL;
    }

    return (value + (align - 1ull)) & ~(align - 1ull);
}

static inline u64 svm_align_down(u64 value, u64 align)
{
    if (align == 0) {
        return value;
    }

    return value & ~(align - 1ull);
}

static inline u64 svm_get_align_up_size(u64 start, u64 size, u64 align)
{
    if (align == 0) {
        return size;
    }

    return (size == 0ULL) ? 0ULL : svm_align_up(size + start % align, align);
}

static inline u64 svm_get_align_up_num(u64 start, u64 size, u64 align)
{
    return svm_get_align_up_size(start, size, align) / align;
}

static inline int svm_check_power_of_2(u64 n)
{
    return (n != 0 && ((n & (n - 1ULL)) == 0)) ? 0 : -1;
}

/* range: [start, end], check [va, va+size] */
static inline int svm_check_va_range(u64 va, u64 size, u64 start, u64 end)
{
    return ((va >= start) && (va < end) && ((va + size) <= end) && (size <= (end - start)) && (size > 0)) ? 0 : -1;
}

enum svm_cpy_dir {
    SVM_H2H_CPY,
    SVM_H2D_CPY,
    SVM_D2H_CPY,
    SVM_D2D_CPY,
    SVM_MAX_CPY_DIR
};

#endif
