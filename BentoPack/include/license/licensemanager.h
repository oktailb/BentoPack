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
