/*
<***>
pmu_mfd.h
Penguin Memory Utils Memory-Mapped File.
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
#ifndef PMU_MFD_H
#define PMU_MFD_H

#include <inttypes.h>
#include <stddef.h>

/// @brief A memory-mapped file handle.
///
/// Populated by pmu_mfd_open. Fields are only valid after a successful open.
/// pmu_mfd_close releases the mapping.
///
/// @warning Do not construct manually. Only pmu_mfd_open may populate this.
///
/// @see pmu_mfd_open
/// @see pmu_mfd_close
typedef struct
{
    void* base;  ///< First byte of the mapping. NULL when not open.
    size_t size; ///< Length of the mapping in bytes. 0 when not open.
} pmu_mfd_t;

/// @brief Memory-map a regular file.
///
/// Opens `path`, verifies it is a regular file, and maps its entire contents
/// into the process address space with mmap(MAP_SHARED). The file descriptor
/// is closed before returning; the mapping remains valid until pmu_mfd_close.
///
/// @param mfd      Pointer to the handle to populate. Must not be NULL.
/// @param path     Path to the file to map. Must not be NULL. Must be a
///                 regular file (not a directory, device, pipe, etc.).
/// @param writable If non-zero, map read/write (O_RDWR, PROT_READ|PROT_WRITE).
///                 If zero, map read-only (O_RDONLY, PROT_READ). In writable
///                 mode, changes are written back to the file (MAP_SHARED).
///
/// @return 0 on success, -1 on failure with errno set.
///
/// @warning The file size is captured at open time. If the file is truncated
///          by another process afterward, accessing the mapped pages may
///          deliver SIGBUS.
/// @warning In writable mode, MAP_SHARED means stores go to the page cache
///          and are eventually written back. Use msync if you need ordering.
/// @note On failure, `mfd->base` is set to NULL and `mfd->size` to 0
///       (provided `mfd` itself is non-NULL).
/// @note The file descriptor is closed before this function returns; the
///       mapping outlives it.
/// @note errno values: EINVAL for NULL mfd/path, non-regular file, or empty
///       file; otherwise whatever open/fstat/mmap set.
int pmu_mfd_open(pmu_mfd_t* mfd, const char* path, int writable);

/// @brief Unmap a memory-mapped file and reset the handle.
///
/// Calls munmap on the mapped region and sets the handle back to the
/// not-open state (`base == NULL`, `size == 0`). Safe to call on an
/// already-closed handle, and safe to call on a zero-initialized handle.
///
/// @param mfd Pointer to the handle. Passing NULL is a no-op.
///
/// @warning All pointers into the previous mapping become invalid. Any
///          access is undefined behavior.
/// @warning In writable mode, data is not explicitly synced before unmapping.
///          munmap flushes dirty pages eventually, but if you need durability
///          before close, call msync first.
/// @note The underlying file is not modified beyond whatever mmap already
///       did; close does not truncate or rename it.
/// @note errno is not modified by this function.
void pmu_mfd_close(pmu_mfd_t* mfd);

#ifdef PMU_MFD_IMPLEMENTATION
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int pmu_mfd_open(pmu_mfd_t* mfd, const char* path, int writable)
{
    if (!mfd)
    {
        errno = EINVAL;
        return -1;
    }

    mfd->base = NULL;
    mfd->size = 0;

    if (!path)
    {
        errno = EINVAL;
        return -1;
    }

    int flags = writable ? O_RDWR : O_RDONLY;
    int fd = open(path, flags);
    if (fd < 0)
        return -1;

    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    if (!S_ISREG(st.st_mode))
    {
        close(fd);
        errno = EINVAL;
        return -1;
    }
    if (st.st_size <= 0)
    {
        close(fd);
        errno = EINVAL;
        return -1;
    }

    size_t size = (size_t)st.st_size;

    int prot = writable ? (PROT_READ | PROT_WRITE) : PROT_READ;
    void* base = mmap(NULL, size, prot, MAP_SHARED, fd, 0);

    if (base == MAP_FAILED)
    {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }

    close(fd);

    mfd->base = base;
    mfd->size = size;
    return 0;
}

void pmu_mfd_close(pmu_mfd_t* mfd)
{
    if (!mfd)
        return;
    if (mfd->base)
    {
        munmap(mfd->base, mfd->size);
    }
    mfd->base = NULL;
    mfd->size = 0;
}
#endif
#endif
