/**
 * @file lv_fs_prefetch.c
 * Prefetch pool implementation — fixed-size array with FIFO eviction.
 */

#include "lv_fs_prefetch.h"
#include "../stdlib/lv_string.h"

static lv_fs_prefetch_entry_t pool[LV_FS_PREFETCH_POOL_SIZE];
static uint32_t seq_counter = 0;

void lv_fs_prefetch_set(const char * path, void * buffer, uint32_t size)
{
    /* Reuse existing entry for same path */
    lv_fs_prefetch_entry_t * entry = lv_fs_prefetch_find(path);
    if(entry) {
        entry->buffer = buffer;
        entry->size = size;
        entry->seq = seq_counter++;
        return;
    }

    /* Find an inactive slot */
    for(int i = 0; i < LV_FS_PREFETCH_POOL_SIZE; i++) {
        if(!pool[i].active) {
            entry = &pool[i];
            break;
        }
    }

    /* All slots occupied — evict the oldest */
    if(entry == NULL) {
        entry = &pool[0];
        for(int i = 1; i < LV_FS_PREFETCH_POOL_SIZE; i++) {
            if(pool[i].seq < entry->seq) {
                entry = &pool[i];
            }
        }
    }

    lv_strlcpy(entry->path, path, sizeof(entry->path));
    entry->buffer = buffer;
    entry->size = size;
    entry->active = true;
    entry->seq = seq_counter++;
}

lv_fs_prefetch_entry_t * lv_fs_prefetch_find(const char * path)
{
    for(int i = 0; i < LV_FS_PREFETCH_POOL_SIZE; i++) {
        if(pool[i].active && lv_strcmp(pool[i].path, path) == 0) {
            return &pool[i];
        }
    }
    return NULL;
}

void lv_fs_prefetch_clear(const char * path)
{
    lv_fs_prefetch_entry_t * entry = lv_fs_prefetch_find(path);
    if(entry) {
        entry->active = false;
    }
}
