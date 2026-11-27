// host.c: the Nibble page's wasm host, the six runtime functions Roc calls,
// over a small allocator on wasm memory. main.roc provides the page's three
// exports itself (`status`, `after` and `accepts`), so this file holds no
// export of the page's.
//
// web/build.shale compiles it with clang (--target=wasm32-unknown-unknown)
// to an object, web/platform/targets/wasm32/host.wasm, and Roc links that
// with the app into out/web/nibble.wasm.
//
// The module imports nothing: the page instantiates it with no import
// object, so a Roc crash (a caller's bug, as an op digit past 3) is a trap,
// which the page sees as a RuntimeError.
//
// Memory is a free list per power of two, from 16 bytes up. Each block
// keeps its size class in the 16 bytes before what Roc is given, for
// `roc_dealloc` and `roc_realloc`; a freed block goes back on its class's
// list. New blocks are carved from the end of the heap, which grows by
// memory.grow as it needs.

#include <stddef.h>

#define HEADER 16
#define CLASSES 28
#define PAGE 65536

extern unsigned char __heap_base;

static size_t top = 0;
static size_t limit = 0;
static void *free_list[CLASSES];

// The smallest class whose blocks hold `total` bytes: 1 << class.
static int class_of(size_t total) {
    int c = 4;
    while (((size_t)1 << c) < total) c++;
    return c;
}

// `bytes` fresh bytes from the end of the heap, grown as it needs.
static void *carve(size_t bytes) {
    if (top == 0) {
        top = ((size_t)&__heap_base + 15) & ~(size_t)15;
        limit = __builtin_wasm_memory_size(0) * PAGE;
    }
    while (top + bytes > limit) {
        size_t pages = (top + bytes - limit + PAGE - 1) / PAGE;
        if (__builtin_wasm_memory_grow(0, pages) == (size_t)-1) __builtin_trap();
        limit += pages * PAGE;
    }
    void *p = (void *)top;
    top += bytes;
    return p;
}

static unsigned char *allocate(size_t len, size_t alignment) {
    if (alignment > HEADER) __builtin_trap();
    int c = class_of(HEADER + len);
    if (c >= CLASSES) __builtin_trap();
    size_t *block = free_list[c];
    if (block) free_list[c] = *(void **)block;
    else block = carve((size_t)1 << c);
    block[0] = (size_t)c;
    return (unsigned char *)block + HEADER;
}

static void release(void *ptr) {
    size_t *block = (size_t *)((unsigned char *)ptr - HEADER);
    int c = (int)block[0];
    *(void **)block = free_list[c];
    free_list[c] = block;
}

void *roc_alloc(size_t len, size_t alignment) {
    return allocate(len, alignment);
}

// A block that already holds `len` bytes is kept as it is.
void *roc_realloc(void *ptr, size_t len, size_t alignment) {
    size_t *block = (size_t *)((unsigned char *)ptr - HEADER);
    size_t was = ((size_t)1 << block[0]) - HEADER;
    if (len <= was) return ptr;
    unsigned char *fresh = allocate(len, alignment);
    __builtin_memcpy(fresh, ptr, was);
    release(ptr);
    return fresh;
}

void roc_dealloc(void *ptr, size_t alignment) {
    (void)alignment;
    release(ptr);
}

void roc_crashed(const unsigned char *bytes, size_t len) {
    (void)bytes;
    (void)len;
    __builtin_trap();
}

void roc_dbg(const unsigned char *bytes, size_t len) {
    (void)bytes;
    (void)len;
}

void roc_expect_failed(const unsigned char *bytes, size_t len) {
    (void)bytes;
    (void)len;
}
