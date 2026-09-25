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

#include "license/licensemanager.h"
#include "license/integrityguard.h"
#include "generated/license_config.h"
#include <QCryptographicHash>

#include <QCoreApplication>

namespace BentoPack {

namespace {

// SHA-256 hash of the authentic commercial release master token.
// Kept in public open-source code; cannot be reversed mathematically.
constexpr const char *EXPECTED_COMMERCIAL_TOKEN_HASH =
    "2b573bbc2ad164c4a295b293fd31d296334f38fa35b4b0184dcc9ad861b88ad3";

static bool s_hasToolTypeOverride = false;
static ToolType s_toolTypeOverride = ToolType::GUI;

bool verifyLicense()
{
#if defined(BENTOPACK_COMMERCIAL_BUILD) && (BENTOPACK_COMMERCIAL_BUILD == 1)
    const char *rawToken = BENTOPACK_LICENSE_TOKEN;
    if (!rawToken || rawToken[0] == '\0') {
        return false;
    }
    QByteArray tokenBytes(rawToken);
    QByteArray hashHex = QCryptographicHash::hash(tokenBytes, QCryptographicHash::Sha256).toHex();
    return (hashHex == QByteArray(EXPECTED_COMMERCIAL_TOKEN_HASH));
#else
    return false;
#endif
}

} // namespace

bool LicenseManager::isCommercial()
{
    static const bool s_isCommercial = verifyLicense();
    return s_isCommercial;
}

Edition LicenseManager::edition()
{
    return isCommercial() ? Edition::Commercial : Edition::Community;
}

QString LicenseManager::editionName()
{
    return isCommercial() ? QStringLiteral("Commercial Edition")
                          : QStringLiteral("Community Edition");
}

ToolType LicenseManager::toolType()
{
    if (s_hasToolTypeOverride) {
        return s_toolTypeOverride;
    }
    if (QCoreApplication::instance()) {
        QString appName = QCoreApplication::applicationName();
        if (appName.contains(QStringLiteral("Cli"), Qt::CaseInsensitive)) {
            return ToolType::CLI;
        }
    }
    return ToolType::GUI;
}

void LicenseManager::setToolType(ToolType type)
{
    s_hasToolTypeOverride = true;
    s_toolTypeOverride = type;
}

QString LicenseManager::toolName()
{
    return (toolType() == ToolType::CLI) ? QStringLiteral("CLI") : QStringLiteral("GUI");
}

QMap<QString, QString> LicenseManager::complianceMetadata()
{
    QMap<QString, QString> meta;
    const bool isCli = (toolType() == ToolType::CLI);
    const QString toolStr = isCli ? QStringLiteral("CLI") : QStringLiteral("GUI");

    if (IntegrityGuard::isTampered()) {
        meta.insert(QStringLiteral("Generator"), QStringLiteral("BentoPack %1 (Tampered Build)").arg(toolStr));
        meta.insert(QStringLiteral("X-BentoPack-Integrity"), QStringLiteral("Tampered-%1-Binary-Circumvention").arg(toolStr));
        meta.insert(QStringLiteral("X-BentoPack-Tool"), toolStr);
        meta.insert(QStringLiteral("X-BentoPack-Notice"),
                    QStringLiteral("UNAUTHORIZED CIRCUMVENTED BUILD - Copyright & DMCA Violation"));
    } else if (isCommercial()) {
        meta.insert(QStringLiteral("Generator"), QStringLiteral("BentoPack %1").arg(toolStr));
        meta.insert(QStringLiteral("X-BentoPack-Edition"), QStringLiteral("Commercial"));
        meta.insert(QStringLiteral("X-BentoPack-Tool"), toolStr);
    } else {
        meta.insert(QStringLiteral("Generator"), QStringLiteral("BentoPack %1 Community Edition").arg(toolStr));
        meta.insert(QStringLiteral("X-BentoPack-Tool"), toolStr);
        meta.insert(QStringLiteral("X-BentoPack-License"), QStringLiteral("Community-Exemption-Under-1M-%1").arg(toolStr));
        if (isCli) {
            meta.insert(QStringLiteral("X-BentoPack-Notice"),
                        QStringLiteral("Evaluation & indie usage (<1M$ revenue exemption). Commercial CLI Automation / CI pipeline license required for automated build pipelines or above threshold."));
        } else {
            meta.insert(QStringLiteral("X-BentoPack-Notice"),
                        QStringLiteral("Evaluation & indie usage (<1M$ revenue exemption). Commercial seat license required above threshold."));
        }
    }
    return meta;
}

void LicenseManager::applyWatermark(QImage &image)
{
    if (image.isNull()) return;

    // Apply steganographic Alpha == 0 pixel watermarking (GUI: SSG, CLI: SSC)
    IntegrityGuard::applySteganographicWatermark(image);

    const bool isCli = (toolType() == ToolType::CLI);
    const QString toolStr = isCli ? QStringLiteral("CLI") : QStringLiteral("GUI");

    if (IntegrityGuard::isTampered()) {
        image.setText(QStringLiteral("Generator"), QStringLiteral("BentoPack %1 (Tampered Build)").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Integrity"), QStringLiteral("Tampered-%1-Binary-Circumvention").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Tool"), toolStr);
        image.setText(QStringLiteral("X-BentoPack-Notice"),
                      QStringLiteral("UNAUTHORIZED CIRCUMVENTED BUILD - Copyright Violation"));
    } else if (isCommercial()) {
        image.setText(QStringLiteral("Generator"), QStringLiteral("BentoPack %1").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Tool"), toolStr);
    } else {
        image.setText(QStringLiteral("Generator"), QStringLiteral("BentoPack %1 Community Edition").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Tool"), toolStr);
        image.setText(QStringLiteral("X-BentoPack-License"), QStringLiteral("Community-Exemption-Under-1M-%1").arg(toolStr));
        if (isCli) {
            image.setText(QStringLiteral("X-BentoPack-Notice"),
                          QStringLiteral("Evaluation & indie usage (<1M$ revenue exemption). Commercial CLI Automation / CI pipeline license required for automated build pipelines or above threshold."));
        } else {
            image.setText(QStringLiteral("X-BentoPack-Notice"),
                          QStringLiteral("Evaluation & indie usage (<1M$ revenue exemption). Commercial seat license required above threshold."));
        }
    }
}

void LicenseManager::applyWatermark(QJsonObject &metaObj)
{
    const bool isCli = (toolType() == ToolType::CLI);
    const QString toolStr = isCli ? QStringLiteral("CLI") : QStringLiteral("GUI");

    if (IntegrityGuard::isTampered()) {
        metaObj[QStringLiteral("app")] = QStringLiteral("BentoPack %1 (Tampered Build)").arg(toolStr);
        metaObj[QStringLiteral("tool")] = toolStr;
        metaObj[QStringLiteral("integrity")] = QStringLiteral("TAMPERED_CIRCUMVENTION_DETECTED");
        metaObj[QStringLiteral("signature")] = QStringLiteral("tampered-tamper-detected");
    } else if (isCommercial()) {
        metaObj[QStringLiteral("app")] = QStringLiteral("BentoPack %1").arg(toolStr);
        metaObj[QStringLiteral("tool")] = toolStr;
        QString imgRef = metaObj.value(QStringLiteral("image")).toString();
        metaObj[QStringLiteral("signature")] = IntegrityGuard::computeLayoutSignature(imgRef);
        metaObj.remove(QStringLiteral("license"));
        metaObj.remove(QStringLiteral("integrity"));
    } else {
        metaObj[QStringLiteral("app")] = QStringLiteral("BentoPack %1 Community Edition").arg(toolStr);
        metaObj[QStringLiteral("tool")] = toolStr;
        metaObj[QStringLiteral("license")] = QStringLiteral("Community-Exemption-Under-1M-%1").arg(toolStr);
        metaObj[QStringLiteral("signature")] = isCli ? QStringLiteral("community-cli-unverified") : QStringLiteral("community-gui-unverified");
    }
}

QString LicenseManager::watermarkHeaderComment()
{
    const bool isCli = (toolType() == ToolType::CLI);
    const QString toolStr = isCli ? QStringLiteral("CLI") : QStringLiteral("GUI");

    if (IntegrityGuard::isTampered()) {
        return QStringLiteral("; WARNING: Generated by Tampered / Circumvented BentoPack %1 binary (Unlicensed / Copyright Violation)\n"
                              "; X-BentoPack-Integrity: Tampered-%1-Binary-Circumvention\n\n").arg(toolStr);
    } else if (isCommercial()) {
        return QStringLiteral("; Generated by BentoPack %1\n\n").arg(toolStr);
    } else {
        if (isCli) {
            return QStringLiteral("; Generated by BentoPack CLI Community Edition (Evaluation & Indie <1M$ Revenue Exemption)\n"
                                  "; Commercial CLI Automation or Enterprise Studio license required for automated CI/CD build pipelines or organizations exceeding 1,000,000$ ARR\n\n");
        } else {
            return QStringLiteral("; Generated by BentoPack GUI Community Edition (Evaluation & Indie <1M$ Revenue Exemption)\n"
                                  "; Commercial seat license required for organizations exceeding 1,000,000$ ARR\n\n");
        }
    }
}

} // namespace BentoPack
