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

#include "license/integrityguard.h"
#include "license/licensemanager.h"
#include "generated/license_config.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

namespace SpriteStudio {

bool IntegrityGuard::s_simulatedTampered = false;

namespace {

constexpr const char *EXPECTED_COMMERCIAL_HASH =
    "2b573bbc2ad164c4a295b293fd31d296334f38fa35b4b0184dcc9ad861b88ad3";

bool verifyCompiledSecretDirect()
{
#if defined(SPRITESTUDIO_COMMERCIAL_BUILD) && (SPRITESTUDIO_COMMERCIAL_BUILD == 1)
    const char *rawToken = SPRITESTUDIO_LICENSE_TOKEN;
    if (!rawToken || rawToken[0] == '\0') {
        return false;
    }
    QByteArray tokenBytes(rawToken);
    QByteArray hashHex = QCryptographicHash::hash(tokenBytes, QCryptographicHash::Sha256).toHex();
    return (hashHex == QByteArray(EXPECTED_COMMERCIAL_HASH));
#else
    return false;
#endif
}

} // namespace

bool IntegrityGuard::isTampered()
{
    if (s_simulatedTampered) {
        return true;
    }

    // Decentralized check: If LicenseManager claims to be commercial,
    // but the compiled secret does not mathematically match the master hash:
    // the binary was patched or circumvented!
    bool declaredCommercial = LicenseManager::isCommercial();
    bool authenticKey = verifyCompiledSecretDirect();

    if (declaredCommercial && !authenticKey) {
        return true;
    }

    return false;
}

bool IntegrityGuard::isCommercialAuthentic()
{
    if (isTampered()) {
        return false;
    }
    return LicenseManager::isCommercial() && verifyCompiledSecretDirect();
}

QRgb IntegrityGuard::transparentBackgroundColor()
{
    if (isCommercialAuthentic()) {
        return CLEAN_COMMERCIAL_ALPHA0;
    }
    const bool isCli = (LicenseManager::toolType() == ToolType::CLI);
    if (isTampered()) {
        return isCli ? MAGIC_TAMPERED_CLI_ALPHA0 : MAGIC_TAMPERED_GUI_ALPHA0;
    }
    return isCli ? MAGIC_COMMUNITY_CLI_ALPHA0 : MAGIC_COMMUNITY_GUI_ALPHA0;
}

void IntegrityGuard::applySteganographicWatermark(QImage &image)
{
    if (image.isNull()) return;

    if (isCommercialAuthentic()) {
        return; // Clean commercial builds do not embed any watermark.
    }

    const bool isCli = (LicenseManager::toolType() == ToolType::CLI);
    const QRgb mark = isTampered()
        ? (isCli ? MAGIC_TAMPERED_CLI_ALPHA0 : MAGIC_TAMPERED_GUI_ALPHA0)
        : (isCli ? MAGIC_COMMUNITY_CLI_ALPHA0 : MAGIC_COMMUNITY_GUI_ALPHA0);

    // Ensure image is strictly non-premultiplied ARGB32 so RGB channels are preserved when Alpha == 0
    if (image.format() != QImage::Format_ARGB32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    const int width = image.width();
    const int height = image.height();

    for (int y = 0; y < height; ++y) {
        QRgb *scanline = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            if (qAlpha(scanline[x]) == 0) {
                scanline[x] = mark;
            }
        }
    }
}

QString IntegrityGuard::computeLayoutSignature(const QString &payload)
{
    if (isTampered()) {
        return QStringLiteral("tampered-tamper-detected");
    }

    if (isCommercialAuthentic()) {
#if defined(SPRITESTUDIO_COMMERCIAL_BUILD) && (SPRITESTUDIO_COMMERCIAL_BUILD == 1)
        QByteArray key = QByteArray(SPRITESTUDIO_LICENSE_TOKEN);
        QByteArray hmac = QMessageAuthenticationCode::hash(
            payload.toUtf8(), key, QCryptographicHash::Sha256).toHex();
        return QStringLiteral("comm-") + QString::fromLatin1(hmac.left(24));
#endif
    }

    Q_UNUSED(payload);
    return QStringLiteral("community-unverified");
}

bool IntegrityGuard::verifyLayoutSignature(const QString &payload, const QString &signature)
{
    if (signature.isEmpty()) {
        return false;
    }

    if (signature == QStringLiteral("community-unverified")) {
        return true;
    }

    if (signature.startsWith(QStringLiteral("tampered"))) {
        return false;
    }

    if (signature.startsWith(QStringLiteral("comm-"))) {
#if defined(SPRITESTUDIO_COMMERCIAL_BUILD) && (SPRITESTUDIO_COMMERCIAL_BUILD == 1)
        QByteArray key = QByteArray(SPRITESTUDIO_LICENSE_TOKEN);
        QByteArray expectedHmac = QMessageAuthenticationCode::hash(
            payload.toUtf8(), key, QCryptographicHash::Sha256).toHex();
        QString expectedSig = QStringLiteral("comm-") + QString::fromLatin1(expectedHmac.left(24));
        return (signature == expectedSig);
#else
        Q_UNUSED(payload);
        return false;
#endif
    }

    return false;
}

void IntegrityGuard::setSimulatedTampered(bool tampered)
{
    s_simulatedTampered = tampered;
}

} // namespace SpriteStudio
