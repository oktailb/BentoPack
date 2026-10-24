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

#ifndef CPU_FEATURES_H
#define CPU_FEATURES_H

#include <QtCore/qglobal.h>
#include "bentopackcore_export.h"

/**
 * @brief Dynamic CPU instruction set and SIMD architecture discovery.
 *
 * Provides safe runtime detection of vector extensions (AVX2, NEON)
 * allowing dynamic dispatch without risking SIGILL on legacy processors.
 */
class BENTOPACK_CORE_EXPORT CpuFeatures
{
public:
    /**
     * @brief Returns true if host CPU supports x86 AVX2 (256-bit SIMD).
     */
    static bool hasAvx2();

    /**
     * @brief Returns true if host CPU supports ARM NEON (128-bit vector).
     */
    static bool hasNeon();

    /**
     * @brief Returns human-readable active SIMD instruction set name.
     */
    static const char* simdArchitectureName();
};

#endif // CPU_FEATURES_H
