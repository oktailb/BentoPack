/**
 * BentoPack Commercial License Gatekeeper
 * 
 * COPYRIGHT NOTICE & END-USER LICENSE AGREEMENT:
 * This software component is proprietary and governed by plugins/LICENSE-PLUGINS.md.
 * Unauthorized copying, modification, reverse engineering, circumvention of
 * technical protection mechanisms, or distribution of this file or derivative
 * works is strictly prohibited and constitutes an infringement of copyright.
 */

#ifndef COMMERCIALGATEKEEPER_H
#define COMMERCIALGATEKEEPER_H

#include "license/igatekeeper.h"
#include <QByteArray>
#include <QString>
#include <memory>

namespace BentoPack {

class CommercialGatekeeper : public ILicenseGatekeeper
{
public:
    explicit CommercialGatekeeper(const QByteArray &token = QByteArray());
    ~CommercialGatekeeper() override = default;

    bool isCommercial() const override;
    QString editionName() const override;
    QByteArray signChallenge(const QByteArray &nonce) const override;
    bool verifyChallenge(const QByteArray &nonce, const QByteArray &signature) const override;
    bool isFeatureUnlocked(quint32 featureId) const override;

private:
    QByteArray m_token;
    bool m_isValid;
};

/**
 * @brief Factory creating an authentic commercial gatekeeper instance.
 */
std::shared_ptr<ILicenseGatekeeper> createCommercialGatekeeper();

} // namespace BentoPack

#endif // COMMERCIALGATEKEEPER_H
