# LVGL Cache Framework Implementation Details

## 1. Introduction

This document provides an in-depth explanation of the LVGL Cache Framework implementation, focusing on the internal mechanisms, algorithms, and data structures used to achieve efficient caching in embedded systems.

## 2. Memory Management

### 2.1 Cache Entry Layout

The cache entry structure is designed to minimize memory overhead while providing all necessary information for cache management:

```c
typedef struct {
    const lv_cache_t * cache;  /* Pointer to the parent cache */
    int32_t ref_cnt;           /* Reference count */
    uint32_t node_size;        /* Size of the entry node */
    bool is_invalid;           /* Validity flag */
    /* The actual data follows this structure in memory */
} lv_cache_entry_t;
```

The actual cached data is stored immediately after the entry structure in memory, allowing for efficient memory usage and avoiding additional pointer indirection.

### 2.2 Reference Counting

The reference counting mechanism is implemented using atomic operations to ensure thread safety:

```c
static inline void lv_cache_entry_inc_ref(lv_cache_entry_t * entry)
{
    lv_atomic_inc(&entry->ref_cnt);
}

static inline void lv_cache_entry_dec_ref(lv_cache_entry_t * entry)
{
    lv_atomic_dec(&entry->ref_cnt);
}
```

This mechanism ensures that cache entries are not freed while they are still in use, even if they are evicted from the cache.

### 2.3 Memory Allocation

The cache framework uses a single memory allocation for each cache entry, which includes both the entry structure and the cached data. This approach reduces memory fragmentation and improves cache performance.

```c
lv_cache_entry_t * lv_cache_entry_create(const lv_cache_t * cache, uint32_t node_size)
{
    lv_cache_entry_t * entry = lv_malloc(sizeof(lv_cache_entry_t) + node_size);
    if(entry == NULL) return NULL;
    
    entry->cache = cache;
    entry->ref_cnt = 0;
    entry->node_size = node_size;
    entry->is_invalid = false;
    
    return entry;
}
```

## 3. LRU Implementation

### 3.1 Red-Black Tree

The red-black tree is used for efficient lookup of cache entries, with a time complexity of O(log n). The tree is implemented using the `lv_rb_t` data structure, which provides the following operations:

- **Insertion**: O(log n) time complexity
- **Deletion**: O(log n) time complexity
- **Lookup**: O(log n) time complexity

The red-black tree is a self-balancing binary search tree that maintains its balance by coloring each node red or black and ensuring that certain properties are maintained during insertions and deletions.

### 3.2 Doubly Linked List

The doubly linked list is used to maintain the order of cache entry usage, with the most recently used entries at the head of the list and the least recently used entries at the tail. The list is implemented using the `lv_ll_t` data structure, which provides the following operations:

- **Insertion at Head**: O(1) time complexity
- **Removal**: O(1) time complexity
- **Move to Head**: O(1) time complexity

The combination of the red-black tree and the doubly linked list allows for efficient lookup and LRU management, making the cache framework suitable for resource-constrained embedded systems.

### 3.3 LRU Update Algorithm

When a cache entry is accessed, it is moved to the head of the linked list to indicate that it was recently used:

```c
static void lru_update(lv_lru_rb_t * lru_rb, lv_rb_node_t * rb_node)
{
    /* Remove from current position in the list */
    lv_ll_remove(&lru_rb->ll, rb_node);
    
    /* Add to the head of the list */
    lv_ll_insert_head(&lru_rb->ll, rb_node);
}
```

This operation has a time complexity of O(1), making it efficient even for frequent cache accesses.

### 3.4 Victim Selection Algorithm

When the cache is full and a new entry needs to be added, the least recently used entry is selected for eviction. This is done by selecting the entry at the tail of the linked list:

```c
static lv_cache_entry_t * lru_get_victim(lv_cache_t * cache, void * user_data)
{
    lv_lru_rb_t * lru_rb = (lv_lru_rb_t *)cache;
    
    /* Get the tail of the list (least recently used) */
    lv_rb_node_t * rb_node = lv_ll_get_tail(&lru_rb->ll);
    if(rb_node == NULL) return NULL;
    
    /* Get the cache entry from the red-black tree node */
    lv_cache_entry_t * entry = LV_CONTAINER_OF(rb_node, lv_lru_rb_node_t, rb_node)->entry;
    
    /* Only select entries with zero reference count */
    if(lv_cache_entry_get_ref(entry) > 0) return NULL;
    
    return entry;
}
```

This operation has a time complexity of O(1), making it efficient even for large caches.

## 4. Thread Safety

### 4.1 Mutex Locks

Each cache instance has a mutex lock that is acquired and released during cache operations to ensure thread safety:

```c
lv_cache_entry_t * lv_cache_acquire(lv_cache_t * cache, const void * key, void * user_data)
{
    lv_mutex_lock(&cache->lock);
    
    /* Cache lookup logic */
    
    lv_mutex_unlock(&cache->lock);
    return entry;
}
```

This approach ensures that cache operations are atomic and prevents race conditions in multi-threaded environments.

### 4.2 Atomic Reference Counting

The reference counting mechanism uses atomic operations to ensure thread safety:

```c
static inline void lv_cache_entry_inc_ref(lv_cache_entry_t * entry)
{
    lv_atomic_inc(&entry->ref_cnt);
}

static inline void lv_cache_entry_dec_ref(lv_cache_entry_t * entry)
{
    lv_atomic_dec(&entry->ref_cnt);
}
```

This approach ensures that reference counts are updated correctly even if multiple threads are accessing the same cache entry simultaneously.

## 5. Cache Classes

### 5.1 Count-based LRU Cache

The count-based LRU cache (`lv_cache_class_lru_rb_count`) limits the number of entries in the cache. It uses the following algorithm for determining if there's enough space for a new entry:

```c
static lv_cache_reserve_cond_t lru_count_reserve_cond(lv_cache_t * cache, const void * key, uint32_t size, void * user_data)
{
    if(cache->size >= cache->max_size) {
        return LV_CACHE_RESERVE_COND_NEED_VICTIM;
    }
    
    return LV_CACHE_RESERVE_COND_OK;
}
```

If the cache is full, it returns `LV_CACHE_RESERVE_COND_NEED_VICTIM` to indicate that an existing entry needs to be evicted before the new entry can be added.

### 5.2 Size-based LRU Cache

The size-based LRU cache (`lv_cache_class_lru_rb_size`) limits the total memory size occupied by the cache. It uses the following algorithm for determining if there's enough space for a new entry:

```c
static lv_cache_reserve_cond_t lru_size_reserve_cond(lv_cache_t * cache, const void * key, uint32_t size, void * user_data)
{
    lv_lru_rb_t * lru_rb = (lv_lru_rb_t *)cache;
    
    /* Get the size of the new entry */
    uint32_t data_size = lru_rb->get_data_size_cb(key, user_data);
    
    /* Check if the entry is too large for the cache */
    if(data_size > cache->max_size) {
        return LV_CACHE_RESERVE_COND_TOO_LARGE;
    }
    
    /* Check if there's enough space in the cache */
    if(cache->size + data_size > cache->max_size) {
        return LV_CACHE_RESERVE_COND_NEED_VICTIM;
    }
    
    return LV_CACHE_RESERVE_COND_OK;
}
```

If the new entry is too large for the cache, it returns `LV_CACHE_RESERVE_COND_TOO_LARGE`. If the cache doesn't have enough space, it returns `LV_CACHE_RESERVE_COND_NEED_VICTIM` to indicate that existing entries need to be evicted before the new entry can be added.

## 6. Image Cache Implementation

### 6.1 Image Cache Data Structure

The image cache uses the following data structure for cache entries:

```c
typedef struct {
    lv_image_src_t src_type;   /* Type of the image source */
    const void * src;           /* Pointer to the image source */
    lv_draw_buf_t * decoded;    /* Pointer to the decoded image data */
} lv_image_cache_data_t;
```

This structure stores both the image source (for lookup) and the decoded image data (the cached resource).

### 6.2 Image Cache Comparison Function

The image cache uses the following comparison function to match cache entries:

```c
static int32_t image_cache_compare_cb(const lv_image_cache_data_t * k1, const lv_image_cache_data_t * k2)
{
    if(k1->src_type != k2->src_type) {
        return (int32_t)k1->src_type - (int32_t)k2->src_type;
    }
    
    if(k1->src != k2->src) {
        return (k1->src > k2->src) ? 1 : -1;
    }
    
    return 0;
}
```

This function first compares the image source types, and if they are the same, it compares the image source pointers.

### 6.3 Image Cache Free Function

The image cache uses the following function to free cache entries:

```c
static void image_cache_free_cb(lv_image_cache_data_t * cached, void * user_data)
{
    if(cached->decoded) {
        lv_draw_buf_destroy(cached->decoded);
    }
}
```

This function frees the decoded image data when a cache entry is removed from the cache.

## 7. Implementation Challenges and Solutions

### 7.1 Memory Management

**Challenge**: Efficient memory management is crucial for embedded systems with limited resources.

**Solution**: The cache framework uses a single memory allocation for each cache entry, which includes both the entry structure and the cached data. This approach reduces memory fragmentation and improves cache performance.

### 7.2 Thread Safety

**Challenge**: Ensuring thread safety without sacrificing performance is a complex task.

**Solution**: The cache framework uses a combination of mutex locks and atomic operations to ensure thread safety. Mutex locks are used for cache operations, while atomic operations are used for reference counting.

### 7.3 Cache Eviction

**Challenge**: Selecting the appropriate entries for eviction when the cache is full.

**Solution**: The cache framework uses the LRU algorithm to select entries for eviction, with the least recently used entries being evicted first. This approach ensures that frequently used entries remain in the cache.

### 7.4 Cache Invalidation

**Challenge**: Ensuring that cache entries are invalidated when the underlying data changes.

**Solution**: The cache framework provides the `lv_cache_drop` and `lv_cache_drop_all` functions to invalidate cache entries. These functions mark entries as invalid, so they will be freed when their reference counts reach zero.

## 8. Summary

The LVGL Cache Framework is implemented using a combination of red-black trees and doubly linked lists to achieve efficient lookup and LRU management. The framework uses a reference counting mechanism to ensure that resources are not released while in use, and it provides thread safety through mutex locks and atomic operations.

The framework supports multiple caching strategies through its modular design, with the count-based LRU cache and the size-based LRU cache being the two main implementations. The image cache is a practical example of how the framework can be used to cache decoded image data, improving rendering performance in embedded graphics systems.