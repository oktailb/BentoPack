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

#include "license/licensemanager.h"
#include "license/igatekeeper.h"
#include "license/integrityguard.h"
#include <QCoreApplication>

namespace BentoPack {

namespace {

class StandardGatekeeper : public ILicenseGatekeeper
{
public:
    bool isCommercial() const override { return true; }
    QString editionName() const override { return QStringLiteral("BentoPack"); }
    QByteArray signChallenge(const QByteArray &nonce) const override { return nonce; }
    bool verifyChallenge(const QByteArray &nonce, const QByteArray &signature) const override { return nonce == signature; }
    bool isFeatureUnlocked(quint32) const override { return true; }
};

static std::shared_ptr<ILicenseGatekeeper> s_activeGatekeeper = std::make_shared<StandardGatekeeper>();
static bool s_hasToolTypeOverride = false;
static ToolType s_toolTypeOverride = ToolType::GUI;

} // namespace

void LicenseManager::setGatekeeper(std::shared_ptr<ILicenseGatekeeper> gk)
{
    s_activeGatekeeper = gk ? std::move(gk) : std::make_shared<StandardGatekeeper>();
}

std::shared_ptr<ILicenseGatekeeper> LicenseManager::gatekeeper()
{
    if (!s_activeGatekeeper) {
        s_activeGatekeeper = std::make_shared<StandardGatekeeper>();
    }
    return s_activeGatekeeper;
}

bool LicenseManager::isFeatureUnlocked(quint32 featureId)
{
    return gatekeeper()->isFeatureUnlocked(featureId);
}

bool LicenseManager::isCommercial()
{
    return true;
}

Edition LicenseManager::edition()
{
    return Edition::Commercial;
}

QString LicenseManager::editionName()
{
    return QStringLiteral("BentoPack");
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
    const QString toolStr = toolName();
    meta.insert(QStringLiteral("Software"), QStringLiteral("BentoPack %1").arg(toolStr));
    meta.insert(QStringLiteral("Comment"), QStringLiteral("Created with BentoPack (https://github.com/oktailb/BentoPack)"));
    return meta;
}

void LicenseManager::applyWatermark(QImage &image)
{
    if (image.isNull()) return;

    const QString toolStr = toolName();
    // Standard promotional & attribution EXIF/PNG text chunks ("Made with BentoPack")
    image.setText(QStringLiteral("Software"), QStringLiteral("BentoPack %1").arg(toolStr));
    image.setText(QStringLiteral("Generator"), QStringLiteral("BentoPack %1").arg(toolStr));
    image.setText(QStringLiteral("Comment"), QStringLiteral("Created with BentoPack - High-density 2D sprite studio (https://github.com/oktailb/BentoPack)"));
}

void LicenseManager::applyWatermark(QJsonObject &metaObj)
{
    const QString toolStr = toolName();
    metaObj[QStringLiteral("app")] = QStringLiteral("BentoPack %1").arg(toolStr);
    metaObj[QStringLiteral("url")] = QStringLiteral("https://github.com/oktailb/BentoPack");
    metaObj[QStringLiteral("signature")] = QStringLiteral("bentopack");
}

QString LicenseManager::watermarkHeaderComment()
{
    const QString toolStr = toolName();
    return QStringLiteral("; Created with BentoPack %1 (https://github.com/oktailb/BentoPack)\n\n").arg(toolStr);
}

} // namespace BentoPack
