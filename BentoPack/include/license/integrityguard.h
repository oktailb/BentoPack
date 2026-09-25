/**
 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing,
 software distributed under the License is distributed on an
 "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND, either express or implied.  See the License for the
 specific language governing permissions and limitations
 under the License.
*/

#ifndef INTEGRITYGUARD_H
#define INTEGRITYGUARD_H

#include "bentopackcore_export.h"
#include <QString>
#include <QImage>
#include <QRgb>

namespace BentoPack {

/**
 * @brief Decentralized Anti-Tamper & Forensic Integrity Guard.
 *
 * Protects against source-code circumvention, unlicensed re-compilation,
 * and commercial status spoofing. Injects steganographic alpha-zero watermarks
 * and verifies layout HMAC signatures.
 */
class BENTOPACK_CORE_EXPORT IntegrityGuard
{
public:
    // Steganographic magic 32-bit pixel patterns for Alpha == 0 pixels:
    // Community GUI: 'S', 'S', 'G', alpha=0 -> 0x00535347
    static constexpr QRgb MAGIC_COMMUNITY_GUI_ALPHA0 = 0x00535347;
    // Community CLI: 'S', 'S', 'C', alpha=0 -> 0x00535343
    static constexpr QRgb MAGIC_COMMUNITY_CLI_ALPHA0 = 0x00535343;
    // Legacy alias (defaults to GUI)
    static constexpr QRgb MAGIC_COMMUNITY_ALPHA0     = MAGIC_COMMUNITY_GUI_ALPHA0;

    // Tampered / Circumvented GUI: 'S', 'S', 'T', alpha=0 -> 0x00535354
    static constexpr QRgb MAGIC_TAMPERED_GUI_ALPHA0  = 0x00535354;
    // Tampered / Circumvented CLI: 'S', 'S', 'X', alpha=0 -> 0x00535358
    static constexpr QRgb MAGIC_TAMPERED_CLI_ALPHA0  = 0x00535358;
    // Legacy alias (defaults to GUI)
    static constexpr QRgb MAGIC_TAMPERED_ALPHA0      = MAGIC_TAMPERED_GUI_ALPHA0;

    // Authentic clean commercial: 0, 0, 0, 0 -> 0x00000000
    static constexpr QRgb CLEAN_COMMERCIAL_ALPHA0    = 0x00000000;

    /**
     * @brief Checks if the running binary was tampered with (e.g. patched isCommercial()
     *        without having the authentic cryptographic secret token at build time).
     */
    static bool isTampered();

    /**
     * @brief Checks if the running build is an authentic commercial release.
     */
    static bool isCommercialAuthentic();

    /**
     * @brief Returns the transparent fill color (QRgb) for atlas generation.
     *        In Community/Tampered builds, embeds the steganographic mark in transparent pixels.
     */
    static QRgb transparentBackgroundColor();

    /**
     * @brief Embeds the steganographic forensic watermark in all Alpha == 0 pixels.
     *        Does nothing in authentic commercial builds.
     */
    static void applySteganographicWatermark(QImage &image);

    /**
     * @brief Computes a layout HMAC-SHA256 signature for exported data.
     */
    static QString computeLayoutSignature(const QString &payload);

    /**
     * @brief Verifies whether a given signature is authentic.
     */
    static bool verifyLayoutSignature(const QString &payload, const QString &signature);

    /**
     * @brief Simulates a tampered state for unit testing and forensic validation.
     */
    static void setSimulatedTampered(bool tampered);

private:
    static bool s_simulatedTampered;
};

} // namespace BentoPack

#endif // INTEGRITYGUARD_H
