/**
 * Copyright (c) 2026 Vincent LECOQ
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "simd/cpu_features.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#  define ARCH_X86 1
#  if defined(_MSC_VER)
#    include <intrin.h>
#  endif
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(__ARM_NEON) || defined(__ARM_NEON__)
#  define ARCH_ARM 1
#endif

bool CpuFeatures::hasAvx2()
{
#if defined(ARCH_X86)
#  if defined(__GNUC__) || defined(__clang__)
#    if __has_builtin(__builtin_cpu_supports)
    return __builtin_cpu_supports("avx2");
#    else
    return false;
#    endif
#  elif defined(_MSC_VER)
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 0);
    if (cpuInfo[0] < 7) return false;
    __cpuidex(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 5)) != 0; // AVX2 is bit 5 of EBX on Leaf 7
#  else
    return false;
#  endif
#else
    return false;
#endif
}

bool CpuFeatures::hasNeon()
{
#if defined(ARCH_ARM)
#  if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__) || defined(_M_ARM64)
    return true;
#  else
    return false;
#  endif
#else
    return false;
#endif
}

const char* CpuFeatures::simdArchitectureName()
{
    if (hasAvx2()) {
        return "AVX2 (256-bit SIMD)";
    }
    if (hasNeon()) {
        return "ARM NEON (128-bit SIMD)";
    }
    return "Scalar (Optimized direct scanlines)";
}
