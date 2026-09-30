/**
 * BentoPack Forensic Integrity & Anti-Tamper Subsystem
 *
 * COPYRIGHT NOTICE & END-USER LICENSE AGREEMENT:
 * This software component is proprietary and governed by plugins/LICENSE-PLUGINS.md.
 * It implements statutory Copyright Management Information (CMI) and forensic
 * integrity verification protected under 17 U.S.C. § 1202, WIPO Copyright Treaty Art. 12,
 * and EU Directive 2009/24/EC.
 *
 * Unauthorized modification, bypassing, stripping of forensic metadata tags,
 * reverse engineering, or redistribution of this file or derivative works is
 * strictly prohibited and constitutes willful infringement of copyright.
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

    // Clean transparent pixel: 0, 0, 0, 0 -> 0x00000000.
    // Transparent pixels are strictly preserved to guarantee zero color bleeding,
    // perfect mipmapping, and artifact-free GPU block compression (KTX2/UASTC/BC7/ASTC).
    static constexpr QRgb CLEAN_ALPHA0               = 0x00000000;
    static constexpr QRgb CLEAN_COMMERCIAL_ALPHA0    = CLEAN_ALPHA0;

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
     *        Always returns clean transparent 0x00000000 to protect rendering pipelines.
     */
    static QRgb transparentBackgroundColor();

    /**
     * @brief Embeds non-destructive forensic metadata (PNG text chunks, KTX2 dict, HMAC signatures)
     *        into the image without altering pixel colors.
     */
    static void applySteganographicWatermark(QImage &image);

    /**
     * @brief Semantic alias for applySteganographicWatermark.
     */
    static void applyForensicMetadata(QImage &image) { applySteganographicWatermark(image); }

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
