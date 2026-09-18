# PMU — Penguin Memory Utils

Small, dependency-free, header-only memory utilities for Linux x86-64 game code.

Three single-header libraries, one include directory. No build system, no
dependencies beyond the C standard library and the Linux kernel. Drop the
headers into your `include/` folder and define the implementation macro in
exactly one `.c` file per header.

---

## Contents

| Header | Provides |
|---|---|
| `pmu_arena.h` | Bump arena allocator with lazy commit over reserved virtual memory. |
| `pmu_mapfd.h` | Read-only / writable `mmap` wrapper for regular files. |
| `pmu_memsize_defines.h` | `PMU_B` / `PMU_KB` / `PMU_MB` / `PMU_GB` / `PMU_TB` size macros. |

---

## Requirements

- Linux (uses `mmap`, `mprotect`, `munmap`, `sysconf`).
- 64-bit target (x86-64, aarch64, etc.).
- A C99 (or later) compiler.
- Standard C library with `<sys/mman.h>`.

No threads required. No external dependencies.

---

## Installation
Copy the headers into your project's include directory.
In **exactly one** translation unit per header, define the implementation
macro before including it:
```c
/* src/pmu_impl.c */
#define PMU_ARENA_IMPLEMENTATION
#include "pmu_arena.h"
#define PMU_MFD_IMPLEMENTATION
#include "pmu_mapfd.h"
/* pmu_memsize_defines.h has no implementation block; just include it. */
