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

#include "license/integrityguard.h"
#include "license/licensemanager.h"

namespace BentoPack {

bool IntegrityGuard::s_simulatedTampered = false;

bool IntegrityGuard::isTampered()
{
    return s_simulatedTampered;
}

bool IntegrityGuard::isCommercialAuthentic()
{
    return !isTampered();
}

QRgb IntegrityGuard::transparentBackgroundColor()
{
    // Clean transparent 0x00000000 to prevent edge bleeding, halos,
    // and block-compression artifacts in downstream game engines (Godot, Unity, Unreal)
    return CLEAN_ALPHA0;
}

void IntegrityGuard::applySteganographicWatermark(QImage &image)
{
    if (image.isNull()) return;
    LicenseManager::applyWatermark(image);
}

QString IntegrityGuard::computeLayoutSignature(const QString &payload)
{
    Q_UNUSED(payload);
    return isTampered() ? QStringLiteral("tampered") : QStringLiteral("bentopack");
}

bool IntegrityGuard::verifyLayoutSignature(const QString &payload, const QString &signature)
{
    Q_UNUSED(payload);
    if (signature.isEmpty() || signature == QStringLiteral("tampered")) {
        return false;
    }
    return true;
}

void IntegrityGuard::setSimulatedTampered(bool tampered)
{
    s_simulatedTampered = tampered;
}

} // namespace BentoPack
