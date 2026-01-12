/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IPC_PINGPONG_H
#define IPC_PINGPONG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <zephyr/sys/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory Layout Constants */
#define SHARED_MEM_BASE     0x20010000UL
#define BUFFER_SIZE         (64 * 1024)
#define CONTROL_BLOCK_SIZE  (32 * 1024)

/* Buffer addresses */
#define BUFFER_0_ADDR       (SHARED_MEM_BASE)
#define BUFFER_1_ADDR       (SHARED_MEM_BASE + BUFFER_SIZE)
#define CONTROL_BLOCK_ADDR  (SHARED_MEM_BASE + 2 * BUFFER_SIZE)

/* Buffer States */
typedef enum {
    BUFFER_STATE_IDLE = 0,
    BUFFER_STATE_WRITING,
    BUFFER_STATE_READY,
    BUFFER_STATE_READING
} buffer_state_t;

/* Ensure buffer_state_t is 32-bit for atomic operations */
_Static_assert(sizeof(buffer_state_t) <= sizeof(atomic_val_t), 
               "buffer_state_t must fit in atomic_val_t");

/* Error Codes */
#define BUFFER_OK              0
#define BUFFER_ERR_TIMEOUT    -1
#define BUFFER_ERR_INVALID    -2
#define BUFFER_ERR_STATE      -3
#define BUFFER_ERR_OVERRUN    -4
#define BUFFER_ERR_INIT       -5

/* Memory Barrier Macros */
#if defined(__ARM_ARCH)
    /* ARM Cortex-M33 memory barriers */
    #define MEMORY_BARRIER_WRITE() __asm__ volatile("dmb st" ::: "memory")
    #define MEMORY_BARRIER_READ()  __asm__ volatile("dmb ld" ::: "memory")
    #define MEMORY_BARRIER_FULL()  __asm__ volatile("dmb sy" ::: "memory")
#elif defined(__riscv)
    /* RISC-V memory barriers */
    #define MEMORY_BARRIER_WRITE() __asm__ volatile("fence ow, ow" ::: "memory")
    #define MEMORY_BARRIER_READ()  __asm__ volatile("fence ir, ir" ::: "memory")
    #define MEMORY_BARRIER_FULL()  __asm__ volatile("fence iorw, iorw" ::: "memory")
#else
    /* Generic compiler barriers as fallback */
    #define MEMORY_BARRIER_WRITE() __asm__ volatile("" ::: "memory")
    #define MEMORY_BARRIER_READ()  __asm__ volatile("" ::: "memory")
    #define MEMORY_BARRIER_FULL()  __asm__ volatile("" ::: "memory")
#endif

/* Control Block Structure */
typedef struct {
    /* Buffer states (use uint32_t for atomic operations, aligned to 64 bytes) */
    volatile uint32_t buffer_states[2] __attribute__((aligned(64)));
    
    /* Transfer counters (aligned to 4 bytes) */
    volatile uint32_t write_count[2] __attribute__((aligned(4)));
    volatile uint32_t read_count[2] __attribute__((aligned(4)));
    
    /* Error counters (aligned to 4 bytes) */
    volatile uint32_t overrun_count __attribute__((aligned(4)));
    volatile uint32_t timeout_count __attribute__((aligned(4)));
    
    /* Timestamps (for debugging, aligned to 8 bytes) */
    volatile uint64_t last_write_ts[2] __attribute__((aligned(8)));
    volatile uint64_t last_read_ts[2] __attribute__((aligned(8)));
    
    /* Synchronization flags (aligned to 4 bytes) */
    volatile uint32_t flpr_ready __attribute__((aligned(4)));
    volatile uint32_t m33_ready __attribute__((aligned(4)));
    
    /* Configuration (aligned to 4 bytes) */
    uint32_t buffer_size __attribute__((aligned(4)));
    uint32_t timeout_ms __attribute__((aligned(4)));
    
    /* Reserved for future use */
    uint8_t reserved[31744];
} control_block_t;

/* Global control block pointer */
#define CONTROL_BLOCK ((control_block_t *)CONTROL_BLOCK_ADDR)

/* Buffer Handle Structure */
typedef struct {
    uint8_t id;                          /* Buffer ID (0 or 1) */
    void *data;                          /* Pointer to buffer data */
    size_t size;                         /* Buffer size (64KB) */
    volatile uint32_t *state;            /* Pointer to state in control block */
} buffer_handle_t;

/* Statistics Structure */
typedef struct {
    /* Per-buffer statistics */
    uint32_t writes[2];
    uint32_t reads[2];
    uint64_t last_write_ts[2];
    uint64_t last_read_ts[2];
    
    /* Error statistics */
    uint32_t overruns;
    uint32_t timeouts;
    uint32_t state_errors;
    
    /* Performance metrics */
    uint32_t avg_write_latency_us;
    uint32_t avg_read_latency_us;
    uint32_t max_latency_us;
} buffer_stats_t;

/* Atomic State Transition Functions */
static inline bool atomic_cas_state(
    volatile uint32_t *state,
    buffer_state_t expected,
    buffer_state_t desired)
{
    /* Use Zephyr's atomic API for portability across architectures */
    atomic_val_t exp_val = (atomic_val_t)expected;
    return atomic_cas((atomic_t *)state, exp_val, (atomic_val_t)desired);
}

/* Query buffer state (non-blocking) */
static inline buffer_state_t buffer_get_state(uint8_t buffer_id)
{
    if (buffer_id > 1) {
        return BUFFER_STATE_IDLE;  /* Invalid buffer ID */
    }
    /* Use Zephyr's atomic API for safe read */
    atomic_val_t state = atomic_get((atomic_t *)&CONTROL_BLOCK->buffer_states[buffer_id]);
    return (buffer_state_t)state;
}

/* Function Prototypes */

/* Initialize the buffer manager */
int buffer_manager_init(void);

/* FLPR: Acquire next available buffer for writing */
int buffer_acquire_for_write(buffer_handle_t *handle, uint32_t timeout_ms);

/* FLPR: Commit written buffer and notify M33 */
int buffer_commit(buffer_handle_t *handle);

/* M33: Acquire next ready buffer for reading */
int buffer_acquire_for_read(buffer_handle_t *handle, uint32_t timeout_ms);

/* M33: Release processed buffer and notify FLPR */
int buffer_release(buffer_handle_t *handle);

/* Get statistics */
void buffer_get_stats(buffer_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* IPC_PINGPONG_H */
