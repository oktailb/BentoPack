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

namespace BentoPack {

bool IntegrityGuard::s_simulatedTampered = false;

namespace {

constexpr const char *EXPECTED_COMMERCIAL_HASH =
    "2b573bbc2ad164c4a295b293fd31d296334f38fa35b4b0184dcc9ad861b88ad3";

bool verifyCompiledSecretDirect()
{
#if defined(BENTOPACK_COMMERCIAL_BUILD) && (BENTOPACK_COMMERCIAL_BUILD == 1)
    const char *rawToken = BENTOPACK_LICENSE_TOKEN;
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
    // Always return clean transparent 0x00000000 to prevent edge bleeding, halos,
    // and block-compression artifacts in downstream game engines (Godot, Unity, Unreal)
    return CLEAN_COMMERCIAL_ALPHA0;
}

void IntegrityGuard::applySteganographicWatermark(QImage &image)
{
    if (image.isNull()) return;

    const bool isCli = (LicenseManager::toolType() == ToolType::CLI);
    const QString toolStr = isCli ? QStringLiteral("CLI") : QStringLiteral("GUI");

    if (isTampered()) {
        image.setText(QStringLiteral("Generator"), QStringLiteral("BentoPack %1 (Tampered Build)").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Integrity"), QStringLiteral("Tampered-Binary-Circumvention"));
        image.setText(QStringLiteral("X-BentoPack-Tool"), toolStr);
        image.setText(QStringLiteral("X-BentoPack-Notice"),
                      QStringLiteral("UNAUTHORIZED CIRCUMVENTED BUILD - Copyright Violation"));
    } else if (isCommercialAuthentic()) {
        image.setText(QStringLiteral("Generator"), QStringLiteral("BentoPack %1").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Edition"), QStringLiteral("Commercial"));
        image.setText(QStringLiteral("X-BentoPack-Tool"), toolStr);
    } else {
        image.setText(QStringLiteral("Generator"), QStringLiteral("BentoPack %1 Community Edition").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Tool"), toolStr);
        image.setText(QStringLiteral("X-BentoPack-Integrity"), QStringLiteral("Community-Forensic-Traceable"));
        image.setText(QStringLiteral("X-BentoPack-License"), QStringLiteral("Community-Exemption-Under-1M-%1").arg(toolStr));
        image.setText(QStringLiteral("X-BentoPack-Notice"),
                      QStringLiteral("Evaluation & indie usage (<1M$ revenue exemption). Commercial seat license required above threshold."));
    }
}

QString IntegrityGuard::computeLayoutSignature(const QString &payload)
{
    if (isTampered()) {
        return QStringLiteral("tampered-tamper-detected");
    }

    if (isCommercialAuthentic()) {
#if defined(BENTOPACK_COMMERCIAL_BUILD) && (BENTOPACK_COMMERCIAL_BUILD == 1)
        QByteArray key = QByteArray(BENTOPACK_LICENSE_TOKEN);
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
#if defined(BENTOPACK_COMMERCIAL_BUILD) && (BENTOPACK_COMMERCIAL_BUILD == 1)
        QByteArray key = QByteArray(BENTOPACK_LICENSE_TOKEN);
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

} // namespace BentoPack
