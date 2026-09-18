/*
<***>
pmu_memsize_defines.h
Penguin Memory Utils Memory Size Defines.
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
#ifndef PMU_MEM_SIZE_DEFINES_H
#define PMU_MEM_SIZE_DEFINES_H

/// @brief PMU byte macro.
#define PMU_B(x) ((x) * 1ULL)
/// @brief PMU Kilobyte macro.
#define PMU_KB(x) ((x) * 1024ULL)
/// @brief PMU Megabyte macro.
#define PMU_MB(x) ((x) * 1024ULL * 1024ULL)
/// @brief PMU Gigabyte macro.
#define PMU_GB(x) ((x) * 1024ULL * 1024ULL * 1024ULL)
/// @brief PMU Terabyte macro.
#define PMU_TB(x) ((x) * 1024ULL * 1024ULL * 1024ULL * 1024ULL)
#endif
