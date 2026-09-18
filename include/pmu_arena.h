/*
<***>
pmu_arena.h
Penguin Memory Utils Arena Allocator.
MIT License
Copyright (c) 2026 Penguins🐧
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
<***>
*/
#ifndef PMU_ARENA_H
#define PMU_ARENA_H

#include <inttypes.h>
#include <stddef.h>
/// @brief Arena allocator state.
///
/// A bump allocator backed by a single mmap'd region. The full region is
/// reserved up front with PROT_NONE; pages are committed (mprotect RW) on
/// demand as allocations grow. Nothing is ever freed individually — memory
/// is reclaimed by reset (reuse from the start) or destroy (unmap).
///
/// Treat this struct as opaque. Read the fields if you want stats, but do
/// not modify them by hand.
typedef struct
{
    uint8_t* buffer;                ///< Base of the reserved region. NULL if not created / after destroy.
    size_t offset;                  ///< Current bump offset in bytes from `buffer`. Next allocation starts here.
    size_t capacity;                ///< Bytes currently committed (RW). Always page-aligned. `offset <= capacity`.
    size_t reserved_virtual_memory; ///< Total bytes reserved via mmap. Always page-aligned. `capacity <=
                                    ///< reserved_virtual_memory`.
    size_t allocation_count;        ///< Number of successful pmu_arena_allocate calls since last reset.
    size_t reset_count;             ///< Number of pmu_arena_reset calls since creation.
} pmu_arena_t;
/// @brief Create an arena.
///
/// Reserves `reserved_virtual_memory` bytes of virtual address space with
/// mmap(PROT_NONE) and immediately commits (mprotect RW) the first
/// `initial_capacity` bytes. Further commits happen on demand via
/// pmu_arena_allocate / pmu_arena_reserve, up to the reservation limit.
///
/// @param arena                  Pointer to the arena to initialize.
/// @param initial_capacity       Bytes to commit up front. Rounded up to the
///                               OS page size. If 0, one page is committed.
/// @param reserved_virtual_memory Total address space to reserve. Rounded up
///                               to the OS page size. Must be non-zero.
///
/// @return 0 on success, -1 on failure with errno set (EINVAL, ENOMEM).
///
/// @warning The arena is only valid after a successful call.
/// @warning `initial_capacity` must not be larger than
///          `reserved_virtual_memory` *after* rounding; otherwise creation
///          fails with EINVAL.
/// @note Because both values are rounded up to the page size, an
///       `initial_capacity` that is numerically larger than
///       `reserved_virtual_memory` may still succeed if they round to the
///       same page. On success, `capacity <= reserved_virtual_memory` always
///       holds.
int pmu_arena_create(pmu_arena_t* arena, size_t initial_capacity, size_t reserved_virtual_memory);
/// @brief Reset the arena so its memory can be reused from the beginning.
///
/// Sets the arena offset back to 0, allowing subsequent allocations to
/// overwrite the same memory. Useful for per-frame scratch arenas in
/// renderers / game engines.
///
/// @param arena Pointer to the arena. Passing NULL is a no-op.
///
/// @warning All pointers previously returned by pmu_arena_allocate become
///          invalid after reset; their memory will be reused by later
///          allocations.
///
/// @note `allocation_count` is set to 0.
/// @note `reset_count` is incremented.
/// @note Memory contents are not cleared; old data remains until overwritten.
void pmu_arena_reset(pmu_arena_t* arena);
/// @brief Destroy the arena and release all its virtual memory.
///
/// Unmaps the entire reserved region (both committed and uncommitted
/// portions) back to the OS. After this call the arena struct is zeroed
/// and must be re-created with pmu_arena_create before use.
///
/// @param arena Pointer to the arena. Passing NULL is a no-op.
///
/// @warning All pointers previously returned by pmu_arena_allocate become
///          invalid. Any access to them is undefined behavior.
/// @warning The arena struct itself is not freed; only the memory it
///          managed is. The struct's address remains valid.
/// @note `errno` is not modified by this function.
/// @note Safe to call twice; the second call is a no-op.
void pmu_arena_destroy(pmu_arena_t* arena);
/// @brief Allocate `size` bytes from the arena with the given alignment.
///
/// Bump-allocates from the current offset. If the request does not fit in
/// the currently committed region, the arena commits more pages (up to
/// the reservation limit) via mprotect and then allocates.
///
/// @param arena     Pointer to the arena.
/// @param size      Number of bytes to allocate. Must be > 0.
/// @param alignment Required alignment in bytes. Must be a power of two.
///
/// @return Pointer to `size` usable bytes aligned to `alignment`, or NULL
///         on failure with errno set.
///
/// @warning Returned pointers are aligned to `alignment` relative to the
///          arena base, which is page-aligned by mmap. Absolute alignment
///          is only guaranteed for `alignment <= page_size`.
/// @warning The returned memory is uninitialized. Contents are whatever
///          was there before (or zero from a fresh mmap page).
/// @warning Returned pointers are invalidated by pmu_arena_reset and
///          pmu_arena_destroy. They are never freed individually.
/// @note Allocation is never freed individually; the whole arena is
///       reclaimed by reset or destroy.
/// @note errno values: EINVAL for NULL arena, size == 0, or non-power-of-two
///       alignment; ENOMEM if the request would exceed the reservation.
void* pmu_arena_allocate(pmu_arena_t* arena, size_t size, size_t alignment);
/// @brief Ensure at least `size` bytes of committed capacity are available
///        from the current offset.
///
/// Commits more pages up front so that a subsequent burst of allocations
/// will not each pay the cost of mprotect. Does not advance the offset and
/// does not allocate anything — it only grows the committed region.
///
/// @param arena Pointer to the arena.
/// @param size  Minimum number of bytes that must be available from the
///              current offset. Passing 0 is a no-op success.
///
/// @return 0 on success, -1 on failure with errno set.
///
/// @note This is a capacity hint, not an allocation. No pointer is returned
///       and `arena->offset` is unchanged.
/// @note Because capacity is page-granular, the arena may commit slightly
///       more than requested (rounded up to the page size).
/// @note errno values: EINVAL for NULL arena or invalid arena; ENOMEM if
///       the request would exceed reserved_virtual_memory.
int pmu_arena_reserve(pmu_arena_t* arena, size_t size);
#ifdef PMU_ARENA_IMPLEMENTATION
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
static size_t s_pmu_page_size = 0;

static size_t pmu_page_size(void)
{
    if (s_pmu_page_size == 0)
    {
        long p = sysconf(_SC_PAGESIZE);
        s_pmu_page_size = (p > 0) ? (size_t)p : 4096;
    }
    return s_pmu_page_size;
}

static size_t pmu_align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}
static int pmu_arena_grow(pmu_arena_t* arena, size_t min_capacity)
{
    if (min_capacity <= arena->capacity)
        return 0;

    size_t page = pmu_page_size();
    size_t want = pmu_align_up(min_capacity, page);
    if (want < min_capacity)
    {
        errno = EINVAL;
        return -1;
    }
    if (want > arena->reserved_virtual_memory)
    {
        errno = ENOMEM;
        return -1;
    }

    size_t extra = want - arena->capacity;
    if (mprotect(arena->buffer + arena->capacity, extra, PROT_READ | PROT_WRITE) != 0)
    {
        return -1;
    }

    arena->capacity = want;
    return 0;
}

int pmu_arena_create(pmu_arena_t* arena, size_t initial_capacity, size_t reserved_virtual_memory)
{
    if (!arena)
    {
        errno = EINVAL;
        return -1;
    }
    size_t page = pmu_page_size();
    if (reserved_virtual_memory == 0 || reserved_virtual_memory > SIZE_MAX - page)
    {
        errno = EINVAL;
        return -1;
    }
    if (initial_capacity > SIZE_MAX - page)
    {
        errno = EINVAL;
        return -1;
    }

    reserved_virtual_memory = pmu_align_up(reserved_virtual_memory, page);
    initial_capacity = pmu_align_up(initial_capacity, page);

    if (initial_capacity == 0)
        initial_capacity = page;
    if (initial_capacity > reserved_virtual_memory)
    {
        errno = EINVAL;
        return -1;
    }

    void* base = mmap(NULL, reserved_virtual_memory, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (base == MAP_FAILED)
        return -1;

    if (mprotect(base, initial_capacity, PROT_READ | PROT_WRITE) != 0)
    {
        int saved = errno;
        munmap(base, reserved_virtual_memory);
        errno = saved;
        return -1;
    }

    arena->buffer = (uint8_t*)base;
    arena->offset = 0;
    arena->capacity = initial_capacity;
    arena->reserved_virtual_memory = reserved_virtual_memory;
    arena->allocation_count = 0;
    arena->reset_count = 0;
    return 0;
}

void pmu_arena_reset(pmu_arena_t* arena)
{
    if (!arena)
        return;
    arena->offset = 0;
    arena->allocation_count = 0;
    arena->reset_count++;
}

void pmu_arena_destroy(pmu_arena_t* arena)
{
    if (!arena)
        return;
    if (arena->buffer)
    {
        munmap(arena->buffer, arena->reserved_virtual_memory);
    }
    arena->buffer = NULL;
    arena->offset = 0;
    arena->capacity = 0;
    arena->reserved_virtual_memory = 0;
    arena->allocation_count = 0;
    arena->reset_count = 0;
}

void* pmu_arena_allocate(pmu_arena_t* arena, size_t size, size_t alignment)
{
    if (!arena || !arena->buffer)
    {
        errno = EINVAL;
        return NULL;
    }
    if (size == 0)
    {
        errno = EINVAL;
        return NULL;
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        errno = EINVAL;
        return NULL;
    }
    if (arena->offset > SIZE_MAX - alignment)
    {
        errno = ENOMEM;
        return NULL;
    }
    size_t aligned = pmu_align_up(arena->offset, alignment);
    if (aligned < arena->offset)
    {
        errno = EINVAL;
        return NULL;
    }

    size_t end = aligned + size;
    if (end < aligned)
    {
        errno = ENOMEM;
        return NULL;
    }

    if (end > arena->capacity)
    {
        if (pmu_arena_grow(arena, end) != 0)
            return NULL;
    }

    void* p = arena->buffer + aligned;
    arena->offset = end;
    arena->allocation_count++;
    return p;
}

int pmu_arena_reserve(pmu_arena_t* arena, size_t size)
{
    if (!arena || !arena->buffer)
    {
        errno = EINVAL;
        return -1;
    }
    if (size == 0)
        return 0;

    size_t needed = arena->offset + size;
    if (needed < arena->offset)
    {
        errno = ENOMEM;
        return -1;
    }

    return pmu_arena_grow(arena, needed);
}
#endif
#endif
