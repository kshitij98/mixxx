#include <QtDebug>

#include "effects/effectsbackend.h"
#include "effects/effectsmanager.h"

EffectsBackend::EffectsBackend(QObject* pParent,
                               EffectBackendType type)
        : QObject(pParent),
          m_type(type) {
}

EffectsBackend::~EffectsBackend() {
    m_registeredEffects.clear();
    m_effectIds.clear();
}

void EffectsBackend::registerEffect(const QString& id,
                                    EffectManifestPointer pManifest,
                                    EffectInstantiatorPointer pInstantiator) {
    if (m_registeredEffects.contains(id)) {
        qWarning() << "WARNING: Effect" << id << "already registered";
        return;
    }

    pManifest->setBackendType(m_type);

    m_registeredEffects[id] = RegisteredEffect(pManifest, pInstantiator);
    m_effectIds.append(id);
    emit(effectRegistered(pManifest));
}

const QList<QString> EffectsBackend::getEffectIds() const {
    return m_effectIds;
}

EffectManifestPointer EffectsBackend::getManifest(const QString& effectId) const {
    if (!m_registeredEffects.contains(effectId)) {
        qWarning() << "WARNING: Effect" << effectId << "is not registered.";
        return EffectManifestPointer();
    }
    return m_registeredEffects[effectId].manifest();
}

bool EffectsBackend::canInstantiateEffect(const QString& effectId) const {
    return m_registeredEffects.contains(effectId);
}

bool EffectsBackend::instantiateEffect(EffectsManager* pEffectsManager,
        const QString& effectId, const EffectSlotPointer pEffectSlot,
        const QSet<ChannelHandleAndGroup>& activeChannels) {
    if (!m_registeredEffects.contains(effectId)) {
        qWarning() << "WARNING: Effect" << effectId << "is not registered.";
        return false;
    }
    RegisteredEffect& effectInfo = m_registeredEffects[effectId];

    return pEffectSlot->loadEffectToSlot(pEffectsManager,
                                         effectInfo.manifest(),
                                         effectInfo.initiator(),
                                         activeChannels);
}
