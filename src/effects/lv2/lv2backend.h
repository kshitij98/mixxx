#ifndef LV2BACKEND_H
#define LV2BACKEND_H

#include "effects/defs.h"
#include "effects/effectsbackend.h"
#include "effects/lv2/lv2manifest.h"
#include "preferences/usersettings.h"
#include <lilv-0/lilv/lilv.h>

class LV2Backend : public EffectsBackend {
  public:
    LV2Backend();
    virtual ~LV2Backend();

    EffectBackendType getType() const { return EffectBackendType::LV2; };

    const QList<QString> getEffectIds() const;
    const QSet<QString> getDiscoveredPluginIds() const;
    EffectManifestPointer getManifest(const QString& effectId) const;
    LV2EffectManifestPointer getLV2Manifest(const QString& effectId) const;
    std::unique_ptr<EffectProcessor> createProcessor(const QString& effectId) const;
    bool canInstantiateEffect(const QString& effectId) const;

  private:
    void enumeratePlugins();
    void initializeProperties();
    LilvWorld* m_pWorld;
    QHash<QString, LilvNode*> m_properties;
    QHash<QString, LV2EffectManifestPointer> m_registeredEffects;

    QString debugString() const {
        return "LV2Backend";
    }
};

#endif // LV2BACKEND_H
