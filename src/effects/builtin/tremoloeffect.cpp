#include "effects/builtin/tremoloeffect.h"

namespace {
//  Used to avoid gain discontinuities when changing parameters too fast
constexpr double kMaxGainIncrement = 0.001;
}

// static
QString TremoloEffect::getId() {
    return "org.mixxx.effects.tremolo";
}

// static
EffectManifestPointer TremoloEffect::getManifest() {
    EffectManifestPointer pManifest(new EffectManifest());
    pManifest->setId(getId());
    pManifest->setName(QObject::tr("Tremolo"));
    pManifest->setShortName(QObject::tr("Tremolo"));
    pManifest->setAuthor("The Mixxx Team");
    pManifest->setVersion("1.0");
    pManifest->setDescription(QObject::tr(
        "Cycles the volume up and down"));
    pManifest->setMetaknobDefault(1.0);

    EffectManifestParameterPointer depth = pManifest->addParameter();
    depth->setId("depth");
    depth->setName(QObject::tr("Depth"));
    depth->setShortName(QObject::tr("Depth"));
    depth->setDescription(QObject::tr(
        "How much the effect changes the volume"));
    depth->setControlHint(EffectManifestParameter::ControlHint::KNOB_LINEAR);
    depth->setSemanticHint(EffectManifestParameter::SemanticHint::UNKNOWN);
    depth->setUnitsHint(EffectManifestParameter::UnitsHint::UNKNOWN);
    depth->setDefaultLinkType(EffectManifestParameter::LinkType::LINKED);
    depth->setRange(0, 1, 1);

    EffectManifestParameterPointer rate = pManifest->addParameter();
    rate->setId("rate");
    rate->setName(QObject::tr("Rate"));
    rate->setShortName(QObject::tr("Rate"));
    rate->setDescription(QObject::tr(
        "Rate of the volume changes\n"
        "4 beats - 1/8 beat if tempo is detected\n"
        "1/4 Hz - 8 Hz if no tempo is detected"));
    rate->setControlHint(
        EffectManifestParameter::ControlHint::KNOB_LOGARITHMIC);
    rate->setSemanticHint(EffectManifestParameter::SemanticHint::UNKNOWN);
    rate->setUnitsHint(EffectManifestParameter::UnitsHint::BEATS);
    rate->setRange(1.0/4, 1, 8);

    EffectManifestParameterPointer width = pManifest->addParameter();
    width->setId("width");
    width->setName(QObject::tr("Width"));
    width->setShortName(QObject::tr("Width"));
    width->setDescription(QObject::tr(
        "Width of the volume peak\n"
        "10% - 90% of the effect period"));
    width->setControlHint(EffectManifestParameter::ControlHint::KNOB_LINEAR);
    width->setSemanticHint(EffectManifestParameter::SemanticHint::UNKNOWN);
    width->setUnitsHint(EffectManifestParameter::UnitsHint::UNKNOWN);
    width->setRange(0.1, 0.5, 0.9);

    EffectManifestParameterPointer waveform = pManifest->addParameter();
    waveform->setId("waveform");
    waveform->setName(QObject::tr("Waveform"));
    waveform->setShortName(QObject::tr("Waveform"));
    waveform->setDescription(QObject::tr(
        "Shape of the volume modulation wave\n"
        "Fully left: Square wave\n"
        "Fully right: Sine wave"));
    waveform->setControlHint(
        EffectManifestParameter::ControlHint::KNOB_LOGARITHMIC);
    waveform->setSemanticHint(EffectManifestParameter::SemanticHint::UNKNOWN);
    waveform->setUnitsHint(EffectManifestParameter::UnitsHint::UNKNOWN);
    waveform->setRange(0.005, 0.5, 1);

    EffectManifestParameterPointer phase = pManifest->addParameter();
    phase->setId("phase");
    phase->setName(QObject::tr("Phase"));
    phase->setShortName(QObject::tr("Phase"));
    phase->setDescription(QObject::tr(
        "Shifts the position of the volume peak within the period\n"
        "Fully left: beginning of the effect period\n"
        "Fully right: end of the effect period"));
    phase->setControlHint(
        EffectManifestParameter::ControlHint::KNOB_LINEAR);
    phase->setSemanticHint(EffectManifestParameter::SemanticHint::UNKNOWN);
    phase->setUnitsHint(EffectManifestParameter::UnitsHint::UNKNOWN);
    phase->setRange(0, 0, 1);

    EffectManifestParameterPointer quantize = pManifest->addParameter();
    quantize->setId("quantize");
    quantize->setName(QObject::tr("Quantize"));
    quantize->setShortName(QObject::tr("Quantize"));
    quantize->setDescription(QObject::tr(
        "Round the Rate parameter to the nearest whole division of a beat."));
    quantize->setControlHint(
        EffectManifestParameter::ControlHint::TOGGLE_STEPPING);
    quantize->setSemanticHint(EffectManifestParameter::SemanticHint::UNKNOWN);
    quantize->setUnitsHint(EffectManifestParameter::UnitsHint::UNKNOWN);
    quantize->setRange(0, 1, 1);

    EffectManifestParameterPointer triplet = pManifest->addParameter();
    triplet->setId("triplet");
    triplet->setName(QObject::tr("Triplets"));
    triplet->setShortName(QObject::tr("Triplet"));
    triplet->setDescription(QObject::tr(
        "When the Quantize parameter is enabled, divide the effect period by 3."));
    triplet->setControlHint(
        EffectManifestParameter::ControlHint::TOGGLE_STEPPING);
    triplet->setSemanticHint(EffectManifestParameter::SemanticHint::UNKNOWN);
    triplet->setUnitsHint(EffectManifestParameter::UnitsHint::UNKNOWN);
    triplet->setRange(0, 0, 1);

    return pManifest;
}

void TremoloEffect::loadEngineEffectParameters(
        const QMap<QString, EngineEffectParameterPointer>& parameters) {
    m_pDepthParameter = parameters.value("depth");
    m_pRateParameter = parameters.value("rate");
    m_pWidthParameter = parameters.value("width");
    m_pWaveformParameter = parameters.value("waveform");
    m_pPhaseParameter = parameters.value("phase");
    m_pQuantizeParameter = parameters.value("quantize");
    m_pTripletParameter = parameters.value("triplet");
}

void TremoloEffect::processChannel(
        TremoloState* pState,
        const CSAMPLE* pInput, CSAMPLE* pOutput,
        const mixxx::EngineParameters& bufferParameters,
        const EffectEnableState enableState,
        const GroupFeatureState& groupFeatures) {
    const double width = m_pWidthParameter->value();
    const double smooth = m_pWaveformParameter->value();
    const double depth = m_pDepthParameter->value();

    unsigned int currentFrame = pState->currentFrame;
    double gain = pState->gain;

    const GroupFeatureState& gf = groupFeatures;

    bool quantizeEnabling = !pState->quantizeEnabled
                          && m_pQuantizeParameter->toBool();
    bool tripletDisabling = pState->tripletEnabled
                          && !m_pTripletParameter->toBool();

    if (enableState == EffectEnableState::Enabling
     || quantizeEnabling
     || tripletDisabling) {
        if (gf.has_beat_length_sec && gf.has_beat_fraction) {
            currentFrame = gf.beat_fraction * gf.beat_length_sec * bufferParameters.sampleRate();
        } else {
            currentFrame = 0;
        }
        gain = 0;
    }

    int framePerPeriod;
    double rate = m_pRateParameter->value();
    if (gf.has_beat_length_sec && gf.has_beat_fraction) {
        if (m_pQuantizeParameter->toBool()) {
            int divider = log2(rate);
            rate = pow(2, divider);

            if (m_pTripletParameter->toBool()) {
                rate *= 3.0;
            }
        }
        int framePerBeat = gf.beat_length_sec * bufferParameters.sampleRate();
        framePerPeriod = framePerBeat / rate;
    } else {
        framePerPeriod = bufferParameters.sampleRate() / rate;
    }

    unsigned int phaseOffsetFrame = m_pPhaseParameter->value() * framePerPeriod;
    currentFrame = currentFrame % framePerPeriod;

    for (unsigned int i = 0;
            i < bufferParameters.samplesPerBuffer();
            i += bufferParameters.channelCount()) {
        unsigned int positionFrame = (currentFrame - phaseOffsetFrame);
        positionFrame = positionFrame % framePerPeriod;

        //  Relative position (0 to 1) in the period
        double position = static_cast<double>(positionFrame) / framePerPeriod;

        //  Bend the position according to the width parameter
        //  This maps [0 width] to [0 0.5] and [width 1] to [0.5 1]
        if (position < width) {
            position = 0.5 / width * position;
        } else {
            position = 0.5 + 0.5 * (position - width) / (1 - width);
        }

        //  This is where the magic happens
        //  This function gives the gain to apply for position in [0 1]
        //  Plot the function to get a grasp :
        //  From a sine to a square wave depending on the smooth parameter
        double gainTarget = 1.0 - (depth / 2.0)
                + (atan(sin(2.0 * M_PI * position) / smooth) / (2 * atan(1 / smooth)))
                    * depth;

        if (gainTarget > gain + kMaxGainIncrement) {
            gain += kMaxGainIncrement;
        } else if (gainTarget < gain - kMaxGainIncrement) {
            gain -= kMaxGainIncrement;
        } else {
            gain = gainTarget;
        }

        for (int channel = 0; channel < bufferParameters.channelCount(); channel++) {
            pOutput[i+channel] = gain * pInput[i+channel];
        }

        currentFrame++;
    }

    // Write back channel state
    pState->currentFrame = currentFrame;
    pState->gain = gain;
    pState->quantizeEnabled = m_pQuantizeParameter->toBool();
    pState->tripletEnabled = m_pTripletParameter->toBool();
}
