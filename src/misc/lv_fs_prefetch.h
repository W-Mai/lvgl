/**
 * @file lv_fs_prefetch.h
 * Prefetch pool for async image decoding.
 * Stores file data in RAM so lv_fs_open can serve it without real FS I/O.
 */

#ifndef LV_FS_PREFETCH_H
#define LV_FS_PREFETCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lv_conf_internal.h"
#include "lv_fs.h"

#ifndef LV_FS_PREFETCH_POOL_SIZE
#define LV_FS_PREFETCH_POOL_SIZE 4
#endif

typedef struct {
    char path[LV_FS_MAX_PATH_LENGTH];
    void * buffer;
    uint32_t size;
    bool active;
    uint32_t seq; /**< insertion order for FIFO eviction */
} lv_fs_prefetch_entry_t;

/**
 * Register a buffer in the prefetch pool.
 * If all slots are occupied, the oldest entry is evicted.
 * @param path   file path (e.g. "S:/img/bg.bin")
 * @param buffer pointer to file data in RAM (ownership NOT transferred)
 * @param size   size of the buffer in bytes
 */
void lv_fs_prefetch_set(const char * path, void * buffer, uint32_t size);

/**
 * Find a prefetch entry by path.
 * @param path   file path to look up
 * @return       pointer to the entry, or NULL if not found
 */
lv_fs_prefetch_entry_t * lv_fs_prefetch_find(const char * path);

/**
 * Mark a prefetch entry as inactive.
 * @param path   file path to clear
 */
void lv_fs_prefetch_clear(const char * path);

#ifdef __cplusplus
}
#endif

#endif /* LV_FS_PREFETCH_H */
