#ifndef METRONOMEEFFECT_H
#define METRONOMEEFFECT_H

#include <QMap>

#include "effects/effectprocessor.h"
#include "engine/effects/engineeffect.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/enginefilterpansingle.h"
#include "util/class.h"
#include "util/defs.h"
#include "util/sample.h"
#include "util/types.h"


class MetronomeGroupState final : public EffectState {
  public:
    MetronomeGroupState(const mixxx::EngineParameters& bufferParameters)
      : EffectState(bufferParameters),
        m_framesSinceClickStart(0) {
    }
    ~MetronomeGroupState() {
    }

    unsigned int m_framesSinceClickStart;
};

class MetronomeEffect : public EffectProcessorImpl<MetronomeGroupState> {
  public:
    MetronomeEffect() {};
    virtual ~MetronomeEffect();

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            MetronomeGroupState* pState,
            const CSAMPLE* pInput, CSAMPLE* pOutput,
            const mixxx::EngineParameters& bufferParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

  private:
    EngineEffectParameterPointer m_pBpmParameter;
    EngineEffectParameterPointer m_pSyncParameter;

    DISALLOW_COPY_AND_ASSIGN(MetronomeEffect);
};

#endif /* METRONOMEEFFECT_H */
