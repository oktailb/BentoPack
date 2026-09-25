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
#include "license/igatekeeper.h"
#include "license/integrityguard.h"
#include "commercialgatekeeper.h"
#include "generated/license_config.h"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QCoreApplication>

namespace BentoPack {

namespace {

class DefaultCommunityGatekeeper : public ILicenseGatekeeper
{
public:
    bool isCommercial() const override { return false; }
    QString editionName() const override { return QStringLiteral("Community Edition"); }
    QByteArray signChallenge(const QByteArray &) const override { return QByteArray(); }
    bool verifyChallenge(const QByteArray &, const QByteArray &) const override { return false; }
    bool isFeatureUnlocked(quint32) const override { return false; }
};

static std::shared_ptr<ILicenseGatekeeper> s_activeGatekeeper = nullptr;
static bool s_hasToolTypeOverride = false;
static ToolType s_toolTypeOverride = ToolType::GUI;

} // namespace

void LicenseManager::setGatekeeper(std::shared_ptr<ILicenseGatekeeper> gk)
{
    if (!gk) {
        s_activeGatekeeper = std::make_shared<DefaultCommunityGatekeeper>();
        return;
    }

    if (gk->isCommercial()) {
        const quint64 randVal = QRandomGenerator::system()->generate64();
        const QByteArray nonce = QByteArray::number(randVal, 16);
        const QByteArray sig = gk->signChallenge(nonce);
        if (sig.isEmpty() || !gk->verifyChallenge(nonce, sig)) {
            // Handshake failed: fallback to default community gatekeeper
            s_activeGatekeeper = std::make_shared<DefaultCommunityGatekeeper>();
            return;
        }
    }

    s_activeGatekeeper = std::move(gk);
}

std::shared_ptr<ILicenseGatekeeper> LicenseManager::gatekeeper()
{
    if (!s_activeGatekeeper) {
#if defined(BENTOPACK_COMMERCIAL_BUILD) && (BENTOPACK_COMMERCIAL_BUILD == 1)
        auto commGk = createCommercialGatekeeper();
        if (commGk && commGk->isCommercial()) {
            const quint64 randVal = QRandomGenerator::system()->generate64();
            const QByteArray nonce = QByteArray::number(randVal, 16);
            const QByteArray sig = commGk->signChallenge(nonce);
            if (!sig.isEmpty() && commGk->verifyChallenge(nonce, sig)) {
                s_activeGatekeeper = commGk;
                return s_activeGatekeeper;
            }
        }
#endif
        s_activeGatekeeper = std::make_shared<DefaultCommunityGatekeeper>();
    }
    return s_activeGatekeeper;
}

bool LicenseManager::isFeatureUnlocked(quint32 featureId)
{
    return gatekeeper()->isFeatureUnlocked(featureId);
}

bool LicenseManager::isCommercial()
{
    return gatekeeper()->isCommercial();
}

Edition LicenseManager::edition()
{
    return isCommercial() ? Edition::Commercial : Edition::Community;
}

QString LicenseManager::editionName()
{
    return gatekeeper()->editionName();
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
