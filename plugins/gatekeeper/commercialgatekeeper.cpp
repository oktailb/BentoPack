/**
 * BentoPack Commercial License Gatekeeper
 * 
 * COPYRIGHT NOTICE & END-USER LICENSE AGREEMENT:
 * This software component is proprietary and governed by plugins/LICENSE-PLUGINS.md.
 * Unauthorized copying, modification, reverse engineering, circumvention of
 * technical protection mechanisms, or distribution of this file or derivative
 * works is strictly prohibited and constitutes an infringement of copyright.
 */

#include "commercialgatekeeper.h"
#include "generated/license_config.h"
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>

namespace BentoPack {

namespace {

// Authentic commercial release master digest (SHA-256)
constexpr const char *EXPECTED_COMMERCIAL_HASH =
    "2b573bbc2ad164c4a295b293fd31d296334f38fa35b4b0184dcc9ad861b88ad3";

} // namespace

CommercialGatekeeper::CommercialGatekeeper(const QByteArray &token)
    : m_token(token), m_isValid(false)
{
    if (m_token.isEmpty()) {
#if defined(BENTOPACK_COMMERCIAL_BUILD) && (BENTOPACK_COMMERCIAL_BUILD == 1)
        const char *rawToken = BENTOPACK_LICENSE_TOKEN;
        if (rawToken && rawToken[0] != '\0') {
            m_token = QByteArray(rawToken);
        }
#endif
    }

    if (!m_token.isEmpty()) {
        QByteArray hashHex = QCryptographicHash::hash(m_token, QCryptographicHash::Sha256).toHex();
        m_isValid = (hashHex == QByteArray(EXPECTED_COMMERCIAL_HASH));
    }
}

bool CommercialGatekeeper::isCommercial() const
{
    return m_isValid;
}

QString CommercialGatekeeper::editionName() const
{
    return m_isValid ? QStringLiteral("Commercial Edition") : QStringLiteral("Community Edition");
}

QByteArray CommercialGatekeeper::signChallenge(const QByteArray &nonce) const
{
    if (!m_isValid || nonce.isEmpty()) {
        return QByteArray();
    }
    return QMessageAuthenticationCode::hash(nonce, m_token, QCryptographicHash::Sha256);
}

bool CommercialGatekeeper::verifyChallenge(const QByteArray &nonce, const QByteArray &signature) const
{
    if (!m_isValid || nonce.isEmpty() || signature.isEmpty()) {
        return false;
    }
    QByteArray expected = QMessageAuthenticationCode::hash(nonce, m_token, QCryptographicHash::Sha256);
    return (signature == expected);
}

bool CommercialGatekeeper::isFeatureUnlocked(quint32 /*featureId*/) const
{
    return m_isValid;
}

std::shared_ptr<ILicenseGatekeeper> createCommercialGatekeeper()
{
    return std::make_shared<CommercialGatekeeper>();
}

} // namespace BentoPack
