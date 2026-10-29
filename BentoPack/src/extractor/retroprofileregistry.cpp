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

#include "extractor/retroprofileregistry.h"

RetroProfileRegistry::RetroProfileRegistry(QObject *parent)
    : QObject(parent)
{
}

RetroProfileRegistry& RetroProfileRegistry::instance()
{
    static RetroProfileRegistry s_instance;
    return s_instance;
}

void RetroProfileRegistry::registerProfile(const RetroHardwareProfile &profile)
{
    if (profile.id.isEmpty()) return;
    m_profiles.insert(profile.id, profile);
    emit profilesChanged();
}

void RetroProfileRegistry::unregisterProfile(const QString &id)
{
    if (m_profiles.remove(id) > 0) {
        emit profilesChanged();
    }
}

QList<RetroHardwareProfile> RetroProfileRegistry::profiles() const
{
    return m_profiles.values();
}

RetroHardwareProfile RetroProfileRegistry::profile(const QString &id) const
{
    return m_profiles.value(id);
}

bool RetroProfileRegistry::hasProfile(const QString &id) const
{
    return m_profiles.contains(id);
}

RetroHardwareProfile RetroProfileRegistry::profileByLegacyIndex(int legacyIndex) const
{
    if (legacyIndex > 0) {
        for (const auto &p : m_profiles) {
            if (p.legacyIndex == legacyIndex) {
                return p;
            }
        }
    }
    return RetroHardwareProfile();
}

bool RetroProfileRegistry::hasLegacyIndex(int legacyIndex) const
{
    if (legacyIndex <= 0) return false;
    for (const auto &p : m_profiles) {
        if (p.legacyIndex == legacyIndex) return true;
    }
    return false;
}
