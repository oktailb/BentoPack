/**
 * BentoPack License Management & Compliance Subsystem
 *
 * COPYRIGHT NOTICE & END-USER LICENSE AGREEMENT:
 * This software component is proprietary and governed by plugins/LICENSE-PLUGINS.md.
 * It enforces commercial tiers, gatekeeper verification, and statutory Copyright
 * Management Information (CMI) injection protected under 17 U.S.C. § 1202,
 * WIPO Copyright Treaty Art. 12, and EU Directive 2009/24/EC.
 *
 * Unauthorized modification, bypassing, or redistribution of this file or
 * derivative works is strictly prohibited and constitutes willful infringement of copyright.
 */

#ifndef LICENSEMANAGER_H
#define LICENSEMANAGER_H

#include "bentopackcore_export.h"
#include "license/igatekeeper.h"
#include <QString>
#include <QMap>
#include <QImage>
#include <QJsonObject>
#include <memory>

namespace BentoPack {

enum class Edition {
    Community,
    Commercial
};

enum class ToolType {
    GUI,
    CLI
};

/**
 * @brief Manages compliance, deterministic licensing verification, and
 *        discrete non-destructive metadata watermarking for technical exports.
 */
class BENTOPACK_CORE_EXPORT LicenseManager
{
public:
    /**
     * @brief Checks if the running binary was built with authentic commercial credentials.
     * @return true if commercial edition, false if community edition.
     */
    static bool isCommercial();

    /**
     * @brief Returns current edition enum.
     */
    static Edition edition();

    /**
     * @brief Human-readable name of the running edition.
     */
    static QString editionName();

    /**
     * @brief Current tool context (GUI or CLI).
     */
    static ToolType toolType();

    /**
     * @brief Explicitly sets the tool context (GUI or CLI).
     */
    static void setToolType(ToolType type);

    /**
     * @brief Human-readable tool name ("GUI" or "CLI").
     */
    static QString toolName();

    /**
     * @brief Sets or registers the active gatekeeper.
     * Conducts a mutual challenge-response verification before acceptance.
     */
    static void setGatekeeper(std::shared_ptr<ILicenseGatekeeper> gatekeeper);

    /**
     * @brief Returns active gatekeeper instance.
     */
    static std::shared_ptr<ILicenseGatekeeper> gatekeeper();

    /**
     * @brief Checks if a commercial feature is unlocked via the active gatekeeper.
     */
    static bool isFeatureUnlocked(quint32 featureId);

    /**
     * @brief Returns standardized compliance metadata for technical exports.
     */
    static QMap<QString, QString> complianceMetadata();

    /**
     * @brief Injects compliance watermarking metadata into an atlas QImage (PNG tEXt chunks).
     * @param image QImage reference to be tagged before file save.
     */
    static void applyWatermark(QImage &image);

    /**
     * @brief Injects compliance watermarking metadata into a JSON meta object.
     * @param metaObj QJsonObject reference corresponding to the "meta" block.
     */
    static void applyWatermark(QJsonObject &metaObj);

    /**
     * @brief Returns formatted header comment for Godot / Tres text files.
     */
    static QString watermarkHeaderComment();
};

} // namespace BentoPack

#endif // LICENSEMANAGER_H
