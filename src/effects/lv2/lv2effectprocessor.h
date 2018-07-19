#ifndef LV2EFFECTPROCESSOR_H
#define LV2EFFECTPROCESSOR_H

#include "effects/effectprocessor.h"
#include "effects/lv2/lv2manifest.h"
#include "engine/effects/engineeffectparameter.h"
#include <lilv-0/lilv/lilv.h>
#include "effects/defs.h"
#include "engine/engine.h"

class LV2EffectGroupState final: public EffectState {
  public:
    LV2EffectGroupState(const mixxx::EngineParameters& bufferParameters)
            : EffectState(bufferParameters),
              m_pInstance(nullptr) {
    }
    ~LV2EffectGroupState() {
        if (m_pInstance) {
            lilv_instance_deactivate(m_pInstance);
            lilv_instance_free(m_pInstance);
        }
    }

    LilvInstance* lilvInstance(const LilvPlugin* pPlugin,
            const mixxx::EngineParameters& bufferParameters) {
        if (!m_pInstance) {
            m_pInstance = lilv_plugin_instantiate(
                    pPlugin, bufferParameters.sampleRate(), nullptr);
        }
        return m_pInstance;
    }

  private:
    LilvInstance* m_pInstance;
};

class LV2EffectProcessor final : public EffectProcessorImpl<LV2EffectGroupState> {
  public:
    LV2EffectProcessor(LV2EffectManifestPointer pManifest);
    ~LV2EffectProcessor();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(const ChannelHandle& handle,
                        LV2EffectGroupState* channelState,
                        const CSAMPLE* pInput, CSAMPLE* pOutput,
                        const mixxx::EngineParameters& bufferParameters,
                        const EffectEnableState enableState,
                        const GroupFeatureState& groupFeatures) override;
  private:
    LV2EffectGroupState* createSpecificState(
        const mixxx::EngineParameters& bufferParameters) override;

    LV2EffectManifestPointer m_pManifest;
    QList<EngineEffectParameterPointer> m_engineEffectParameters;
    float* m_inputL;
    float* m_inputR;
    float* m_outputL;
    float* m_outputR;
    float* m_LV2parameters;
    const LilvPlugin* m_pPlugin;
    const QList<int> m_audioPortIndices;
    const QList<int> m_controlPortIndices;
};


#endif // LV2EFFECTPROCESSOR_H
