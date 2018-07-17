#include "effects/lv2/lv2effectprocessor.h"
#include "engine/effects/engineeffect.h"
#include "control/controlobject.h"
#include "util/sample.h"
#include "util/defs.h"

LV2EffectProcessor::LV2EffectProcessor(LV2EffectManifestPointer pManifest)
            : m_pManifest(pManifest),
              m_pPlugin(pManifest->getPlugin()),
              m_audioPortIndices(pManifest->getAudioPortIndices()),
              m_controlPortIndices(pManifest->getControlPortIndices()) {
    m_inputL = new float[MAX_BUFFER_LEN];
    m_inputR = new float[MAX_BUFFER_LEN];
    m_outputL = new float[MAX_BUFFER_LEN];
    m_outputR = new float[MAX_BUFFER_LEN];
}

void LV2EffectProcessor::loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_LV2parameters = new float[parameters.size()];

    // EngineEffect passes the EngineEffectParameters indexed by ID string, which
    // is used directly by built-in EffectProcessorImpl subclasseses to access
    // specific named parameters. However, LV2EffectProcessor::process iterates
    // over the EngineEffectParameters to copy their values to the LV2 control
    // ports. To avoid slow string comparisons in the audio engine thread in
    // LV2EffectProcessor::process, rearrange the QMap of EngineEffectParameters by
    // ID string to an ordered QList.
    for (const auto& pManifestParameter : m_pManifest->parameters()) {
        m_engineEffectParameters.append(parameters.value(pManifestParameter->id()));
    }
}

LV2EffectProcessor::~LV2EffectProcessor() {
    delete[] m_inputL;
    delete[] m_inputR;
    delete[] m_outputL;
    delete[] m_outputR;
    delete[] m_LV2parameters;
}

void LV2EffectProcessor::processChannel(const ChannelHandle& handle,
                        LV2EffectGroupState* channelState,
                        const CSAMPLE* pInput, CSAMPLE* pOutput,
                        const mixxx::EngineParameters& bufferParameters,
                        const EffectEnableState enableState,
                        const GroupFeatureState& groupFeatures) {
    Q_UNUSED(groupFeatures);
    Q_UNUSED(enableState);

    for (int i = 0; i < m_engineEffectParameters.size(); i++) {
        m_LV2parameters[i] = m_engineEffectParameters[i]->value();
    }

    int j = 0;
    for (unsigned int i = 0; i < bufferParameters.samplesPerBuffer(); i += 2) {
        m_inputL[j] = pInput[i];
        m_inputR[j] = pInput[i + 1];
        j++;
    }

    LilvInstance* instance = channelState->lilvInstance(m_pPlugin, bufferParameters);
    lilv_instance_run(instance, bufferParameters.framesPerBuffer());

    j = 0;
    for (unsigned int i = 0; i < bufferParameters.samplesPerBuffer(); i += 2) {
        pOutput[i] = m_outputL[j];
        pOutput[i + 1] = m_outputR[j];
        j++;
    }
}

LV2EffectGroupState* LV2EffectProcessor::createSpecificState(
        const mixxx::EngineParameters& bufferParameters) {
    LV2EffectGroupState* pState = new LV2EffectGroupState(bufferParameters);
    LilvInstance* pInstance = pState->lilvInstance(m_pPlugin, bufferParameters);
    VERIFY_OR_DEBUG_ASSERT(pInstance) {
        return pState;
    }

    if (kEffectDebugOutput) {
        qDebug() << this << "LV2EffectProcessor creating LV2EffectGroupState" << pState;
    }

    if (pInstance) {
        for (int i = 0; i < m_engineEffectParameters.size(); i++) {
            m_LV2parameters[i] = m_engineEffectParameters[i]->value();
            lilv_instance_connect_port(pInstance,
                    m_controlPortIndices[i], &m_LV2parameters[i]);
        }

        // We assume the audio ports are in the following order:
        // input_left, input_right, output_left, output_right
        lilv_instance_connect_port(pInstance, m_audioPortIndices[0], m_inputL);
        lilv_instance_connect_port(pInstance, m_audioPortIndices[1], m_inputR);
        lilv_instance_connect_port(pInstance, m_audioPortIndices[2], m_outputL);
        lilv_instance_connect_port(pInstance, m_audioPortIndices[3], m_outputR);

        lilv_instance_activate(pInstance);
    }
    return pState;
};
