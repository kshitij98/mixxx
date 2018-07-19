#include "effects/lv2/lv2backend.h"
#include "effects/lv2/lv2manifest.h"
#include "effects/lv2/lv2effectprocessor.h"

LV2Backend::LV2Backend() {
    m_pWorld = lilv_world_new();
    initializeProperties();
    lilv_world_load_all(m_pWorld);
    enumeratePlugins();
}

LV2Backend::~LV2Backend() {
    for (LilvNode* node : m_properties) {
        lilv_node_free(node);
    }
    lilv_world_free(m_pWorld);
    m_registeredEffects.clear();
}

void LV2Backend::enumeratePlugins() {
    const LilvPlugins *plugs = lilv_world_get_all_plugins(m_pWorld);
    LILV_FOREACH(plugins, i, plugs) {
        const LilvPlugin *plug = lilv_plugins_get(plugs, i);
        if (lilv_plugin_is_replaced(plug)) {
            continue;
        }
        LV2EffectManifestPointer lv2Manifest(new LV2Manifest(plug, m_properties));
        lv2Manifest->getEffectManifest()->setBackendType(getType());
        m_registeredEffects.insert(lv2Manifest->getEffectManifest()->id(),
                                   lv2Manifest);
    }
}

void LV2Backend::initializeProperties() {
    m_properties["audio_port"] = lilv_new_uri(m_pWorld, LV2_CORE__AudioPort);
    m_properties["input_port"] = lilv_new_uri(m_pWorld, LV2_CORE__InputPort);
    m_properties["output_port"] = lilv_new_uri(m_pWorld, LV2_CORE__OutputPort);
    m_properties["control_port"] = lilv_new_uri(m_pWorld, LV2_CORE__ControlPort);
    m_properties["button_port"] = lilv_new_uri(m_pWorld, LV2_CORE__toggled);
    m_properties["integer_port"] = lilv_new_uri(m_pWorld, LV2_CORE__integer);
    m_properties["enumeration_port"] = lilv_new_uri(m_pWorld, LV2_CORE__enumeration);
}

const QList<QString> LV2Backend::getEffectIds() const {
    QList<QString> availableEffects;
    for (const auto& lv2Manifest : m_registeredEffects) {
        if (lv2Manifest->isValid()) {
            availableEffects.append(lv2Manifest->getEffectManifest()->id());
        }
    }
    return availableEffects;
}

const QSet<QString> LV2Backend::getDiscoveredPluginIds() const {
    return m_registeredEffects.keys().toSet();
}

bool LV2Backend::canInstantiateEffect(const QString& effectId) const {
    if (m_registeredEffects.contains(effectId) &&
        m_registeredEffects[effectId]->isValid()) {
        return true;
    }
    return false;
}

EffectManifestPointer LV2Backend::getManifest(const QString& effectId) const {
    VERIFY_OR_DEBUG_ASSERT(m_registeredEffects.contains(effectId)) {
        return EffectManifestPointer();
    }
    return m_registeredEffects.value(effectId)->getEffectManifest();
}

std::unique_ptr<EffectProcessor> LV2Backend::createProcessor(
        const EffectManifestPointer pManifest) const {
    LV2EffectManifestPointer pLV2Manifest = m_registeredEffects.value(pManifest->id());
    VERIFY_OR_DEBUG_ASSERT(pLV2Manifest) {
        return std::unique_ptr<EffectProcessor>(nullptr);
    }
    return std::unique_ptr<EffectProcessor>(
        new LV2EffectProcessor(pLV2Manifest));
}

LV2EffectManifestPointer LV2Backend::getLV2Manifest(const QString& effectId) const {
    return m_registeredEffects[effectId];
}
