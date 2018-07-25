#include "effects/effectslot.h"
#include "effects/effectxmlelements.h"

#include <QDebug>

#include "effects/defs.h"
#include "control/controlpushbutton.h"
#include "control/controlencoder.h"
#include "control/controlproxy.h"
#include "util/math.h"
#include "util/xml.h"

// The maximum number of effect parameters we're going to support.
const unsigned int kDefaultMaxParameters = 16;

EffectSlot::EffectSlot(const QString& group,
                       EffectsManager* pEffectsManager,
                       const unsigned int iEffectnumber,
                       EngineEffectChain* pEngineEffectChain)
        : m_iEffectNumber(iEffectnumber),
          m_group(group),
          m_pEffectsManager(pEffectsManager),
          m_pEngineEffectChain(pEngineEffectChain),
          m_pEngineEffect(nullptr) {
    VERIFY_OR_DEBUG_ASSERT(m_pEngineEffectChain != nullptr) {
        return;
    }

    m_pControlLoaded = new ControlObject(ConfigKey(m_group, "loaded"));
    m_pControlLoaded->setReadOnly();

    m_pControlNumParameters = new ControlObject(ConfigKey(m_group, "num_parameters"));
    m_pControlNumParameters->setReadOnly();

    m_pControlNumParameterSlots = new ControlObject(ConfigKey(m_group, "num_parameterslots"));
    m_pControlNumParameterSlots->setReadOnly();

    m_pControlNumButtonParameters = new ControlObject(ConfigKey(m_group, "num_button_parameters"));
    m_pControlNumButtonParameters->setReadOnly();

    m_pControlNumButtonParameterSlots = new ControlObject(ConfigKey(m_group, "num_button_parameterslots"));
    m_pControlNumButtonParameterSlots->setReadOnly();

    // Default to disabled to prevent accidental activation of effects
    // at the beginning of a set.
    m_pControlEnabled = new ControlPushButton(ConfigKey(m_group, "enabled"));
    m_pControlEnabled->setButtonMode(ControlPushButton::POWERWINDOW);
    connect(m_pControlEnabled, SIGNAL(valueChanged(double)),
            this, SLOT(updateEngineState()));

    m_pControlNextEffect = new ControlPushButton(ConfigKey(m_group, "next_effect"));
    connect(m_pControlNextEffect, SIGNAL(valueChanged(double)),
            this, SLOT(slotNextEffect(double)));

    m_pControlPrevEffect = new ControlPushButton(ConfigKey(m_group, "prev_effect"));
    connect(m_pControlPrevEffect, SIGNAL(valueChanged(double)),
            this, SLOT(slotPrevEffect(double)));

    // Ignoring no-ops is important since this is for +/- tickers.
    m_pControlEffectSelector = new ControlEncoder(ConfigKey(m_group, "effect_selector"), false);
    connect(m_pControlEffectSelector, SIGNAL(valueChanged(double)),
            this, SLOT(slotEffectSelector(double)));

    m_pControlClear = new ControlPushButton(ConfigKey(m_group, "clear"));
    connect(m_pControlClear, SIGNAL(valueChanged(double)),
            this, SLOT(slotClear(double)));

    for (unsigned int i = 0; i < kDefaultMaxParameters; ++i) {
        addEffectParameterSlot();
        addEffectButtonParameterSlot();
    }

    m_pControlMetaParameter = new ControlPotmeter(ConfigKey(m_group, "meta"), 0.0, 1.0);
    connect(m_pControlMetaParameter, SIGNAL(valueChanged(double)),
            this, SLOT(slotEffectMetaParameter(double)));
    m_pControlMetaParameter->set(0.0);
    m_pControlMetaParameter->setDefaultValue(0.0);

    m_pMetaknobSoftTakeover = new SoftTakeover();

    m_pControlLoaded->forceSet(0.0);
    m_pControlNumParameters->forceSet(0.0);
    m_pControlNumButtonParameters->forceSet(0.0);
}

EffectSlot::~EffectSlot() {
    //qDebug() << debugString() << "destroyed";
    unloadEffect();

    delete m_pControlLoaded;
    delete m_pControlNumParameters;
    delete m_pControlNumParameterSlots;
    delete m_pControlNumButtonParameters;
    delete m_pControlNumButtonParameterSlots;
    delete m_pControlNextEffect;
    delete m_pControlPrevEffect;
    delete m_pControlEffectSelector;
    delete m_pControlClear;
    delete m_pControlEnabled;
    delete m_pControlMetaParameter;
    delete m_pMetaknobSoftTakeover;
}

void EffectSlot::addToEngine(std::unique_ptr<EffectProcessor> pProcessor,
        const QSet<ChannelHandleAndGroup>& activeInputChannels) {
    VERIFY_OR_DEBUG_ASSERT(m_pManifest != nullptr) {
        return;
    }

    VERIFY_OR_DEBUG_ASSERT(m_pEngineEffect == nullptr) {
        return;
    }

    m_pEngineEffect = new EngineEffect(m_pManifest,
            activeInputChannels,
            m_pEffectsManager,
            std::move(pProcessor));

    EffectsRequest* request = new EffectsRequest();
    request->type = EffectsRequest::ADD_EFFECT_TO_CHAIN;
    request->pTargetChain = m_pEngineEffectChain;
    request->AddEffectToChain.pEffect = m_pEngineEffect;
    request->AddEffectToChain.iIndex = m_iEffectNumber;
    m_pEffectsManager->writeRequest(request);
}

void EffectSlot::removeFromEngine() {
    VERIFY_OR_DEBUG_ASSERT(isLoaded()) {
        return;
    }

    EffectsRequest* request = new EffectsRequest();
    request->type = EffectsRequest::REMOVE_EFFECT_FROM_CHAIN;
    request->pTargetChain = m_pEngineEffectChain;
    request->RemoveEffectFromChain.pEffect = m_pEngineEffect;
    request->RemoveEffectFromChain.iIndex = m_iEffectNumber;
    m_pEffectsManager->writeRequest(request);

    m_pEngineEffect = nullptr;
}

void EffectSlot::updateEngineState() {
    if (!m_pEngineEffect) {
        return;
    }
    sendParameterUpdate();
    for (auto const& pParameterSlot : m_knobParameterSlots) {
        pParameterSlot->updateEngineState();
    }
    for (auto const& pButtonParameterSlot : m_buttonParameterSlots) {
        pButtonParameterSlot->updateEngineState();
    }
}

void EffectSlot::sendParameterUpdate() {
    if (!m_pEngineEffect) {
        return;
    }
    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::SET_EFFECT_PARAMETERS;
    pRequest->pTargetEffect = m_pEngineEffect;
    pRequest->SetEffectParameters.enabled = m_pControlEnabled->get();
    m_pEffectsManager->writeRequest(pRequest);
}

EffectState* EffectSlot::createState(const mixxx::EngineParameters& bufferParameters) {
    VERIFY_OR_DEBUG_ASSERT(m_pEngineEffect != nullptr) {
        return new EffectState(bufferParameters);
    }
    return m_pEngineEffect->createState(bufferParameters);
}

EngineEffect* EffectSlot::getEngineEffect() {
    return m_pEngineEffect;
}

EffectManifestPointer EffectSlot::getManifest() const {
    return m_pManifest;
}

void EffectSlot::reload(const QSet<ChannelHandleAndGroup>& activeInputChannels) {
    loadEffect(m_pManifest,
            m_pEffectsManager->createProcessor(m_pManifest),
            activeInputChannels);
}

EffectParameterSlotPointer EffectSlot::addEffectParameterSlot() {
    auto pParameterSlot = EffectParameterSlotPointer(
            new EffectParameterSlot(m_pEffectsManager, m_group, m_knobParameterSlots.size()));
    m_knobParameterSlots.append(pParameterSlot);
    m_pControlNumParameterSlots->forceSet(
            m_pControlNumParameterSlots->get() + 1);
    return pParameterSlot;
}

EffectButtonParameterSlotPointer EffectSlot::addEffectButtonParameterSlot() {
    auto pParameterSlot = EffectButtonParameterSlotPointer(
            new EffectButtonParameterSlot(m_pEffectsManager, m_group, m_buttonParameterSlots.size()));
    m_buttonParameterSlots.append(pParameterSlot);
    m_pControlNumButtonParameterSlots->forceSet(
            m_pControlNumButtonParameterSlots->get() + 1);
    return pParameterSlot;
}

unsigned int EffectSlot::numKnobParameters() const {
    unsigned int num = 0;
    for (const auto& pManifestParameter: m_pManifest->parameters()) {
        if (isKnobParameter(pManifestParameter)) {
            ++num;
        }
    }
    return num;
}

unsigned int EffectSlot::numButtonParameters() const {
    unsigned int num = 0;
    for (const auto& pManifestParameter: m_pManifest->parameters()) {
        if (isButtonParameter(pManifestParameter)) {
            ++num;
        }
    }
    return num;
}

// static
bool EffectSlot::isButtonParameter(EffectManifestParameterPointer pManifestParameter) {
    return  pManifestParameter->controlHint() ==
            EffectManifestParameter::ControlHint::TOGGLE_STEPPING;
}

// static
bool EffectSlot::isKnobParameter(EffectManifestParameterPointer pManifestParameter) {
    return !isButtonParameter(pManifestParameter);
}

EffectManifestParameterPointer EffectSlot::getFilteredParameterForSlot(
        ParameterFilterFnc filterFnc, unsigned int slotNumber) {
    // It's normal to ask for a parameter that doesn't exist. Callers must check
    // for NULL.
    unsigned int num = 0;
    for (const auto& pManifestParameter: m_pManifest->parameters()) {
        if (pManifestParameter->showInParameterSlot() && filterFnc(pManifestParameter)) {
            if(num == slotNumber) {
                return pManifestParameter;
            }
            ++num;
        }
    }
    return EffectManifestParameterPointer();
}

EffectManifestParameterPointer EffectSlot::getKnobParameterForSlot(unsigned int slotNumber) {
    return getFilteredParameterForSlot(isKnobParameter, slotNumber);
}

EffectManifestParameterPointer EffectSlot::getButtonParameterForSlot(unsigned int slotNumber) {
    return getFilteredParameterForSlot(isButtonParameter, slotNumber);
}

double EffectSlot::getMetaknobDefault() {
    return m_pManifest->metaknobDefault();
}

unsigned int EffectSlot::numParameterSlots() const {
    return m_knobParameterSlots.size();
}

unsigned int EffectSlot::numButtonParameterSlots() const {
    return m_buttonParameterSlots.size();
}

void EffectSlot::setEnabled(bool enabled) {
    m_pControlEnabled->set(enabled);
}

EffectParameterSlotPointer EffectSlot::getEffectParameterSlot(unsigned int slotNumber) {
    //qDebug() << debugString() << "getEffectParameterSlot" << slotNumber;
    return m_knobParameterSlots.value(slotNumber, EffectParameterSlotPointer());
}

EffectButtonParameterSlotPointer EffectSlot::getEffectButtonParameterSlot(unsigned int slotNumber) {
    //qDebug() << debugString() << "getEffectParameterSlot" << slotNumber;
    return m_buttonParameterSlots.value(slotNumber, EffectButtonParameterSlotPointer());
}

void EffectSlot::loadEffect(const EffectManifestPointer pManifest,
                            std::unique_ptr<EffectProcessor> pProcessor,
                            const QSet<ChannelHandleAndGroup>& activeChannels) {
    unloadEffect();

    m_pManifest = pManifest;

    if (pManifest == EffectManifestPointer()) {
        // No new effect to load; just unload the old effect and return.
        emit(effectChanged());
        return;
    }

    addToEngine(std::move(pProcessor), activeChannels);

    m_pControlLoaded->forceSet(1.0);

    unsigned int iParameterSlot = 0, iButtonParameterSlot = 0;
    for (const auto& pManifestParameter: m_pManifest->parameters()) {
        if (isKnobParameter(pManifestParameter)) {
            if (iParameterSlot < m_knobParameterSlots.size()) {
                m_knobParameterSlots[iParameterSlot++]->loadManifestParameter(m_pEngineEffect, pManifestParameter);
            }
        } else {
            if (iButtonParameterSlot < m_buttonParameterSlots.size()) {
                m_buttonParameterSlots[iButtonParameterSlot++]->loadManifestParameter(m_pEngineEffect, pManifestParameter);
            }
        }
    }

    if (m_pEffectsManager->isAdoptMetaknobValueEnabled()) {
        slotEffectMetaParameter(m_pControlMetaParameter->get(), true);
    } else {
        m_pControlMetaParameter->set(getMetaknobDefault());
        slotEffectMetaParameter(getMetaknobDefault(), true);
    }

    emit(effectChanged());
    updateEngineState();
}

void EffectSlot::unloadEffect() {
    if (!isLoaded()) {
        return;
    }

    m_pControlLoaded->forceSet(0.0);
    m_pControlNumParameters->forceSet(0.0);
    m_pControlNumButtonParameters->forceSet(0.0);
    for (const auto& pParameterSlot : m_knobParameterSlots) {
        pParameterSlot->clear();
    }
    for (const auto& pButtonParameter : m_buttonParameterSlots) {
        pButtonParameter->clear();
    }
    m_pManifest.clear();

    removeFromEngine();
}

void EffectSlot::slotPrevEffect(double v) {
    if (v > 0) {
        slotEffectSelector(-1);
    }
}

void EffectSlot::slotNextEffect(double v) {
    if (v > 0) {
        slotEffectSelector(1);
    }
}

void EffectSlot::slotEffectSelector(double v) {
    // if (v > 0) {
    //     emit(nextEffect(m_iChainNumber, m_iEffectNumber, m_pEffect));
    // } else if (v < 0) {
    //     emit(prevEffect(m_iChainNumber, m_iEffectNumber, m_pEffect));
    // }
}

void EffectSlot::slotClear(double v) {
    if (v > 0) {
        unloadEffect();
        emit(effectChanged());
    }
}

void EffectSlot::syncSofttakeover() {
    for (const auto& pParameterSlot : m_knobParameterSlots) {
        pParameterSlot->syncSofttakeover();
    }
}

double EffectSlot::getMetaParameter() const {
    return m_pControlMetaParameter->get();
}

// This function is for the superknob to update individual effects' meta knobs
// slotEffectMetaParameter does not need to update m_pControlMetaParameter's value
void EffectSlot::setMetaParameter(double v, bool force) {
    if (!m_pMetaknobSoftTakeover->ignore(m_pControlMetaParameter, v)
            || !m_pControlEnabled->toBool()
            || force) {
        m_pControlMetaParameter->set(v);
        slotEffectMetaParameter(v, force);
    }
}

void EffectSlot::slotEffectMetaParameter(double v, bool force) {
    // Clamp to [0.0, 1.0]
    if (v < 0.0 || v > 1.0) {
        qWarning() << debugString() << "value out of limits";
        v = math_clamp(v, 0.0, 1.0);
        m_pControlMetaParameter->set(v);
    }
    if (!m_pControlEnabled->toBool()) {
        force = true;
    }
    for (const auto& pParameterSlot : m_knobParameterSlots) {
        pParameterSlot->onEffectMetaParameterChanged(v, force);
    }
}

QDomElement EffectSlot::toXml(QDomDocument* doc) const {
    QDomElement effectElement = doc->createElement(EffectXml::Effect);
    // if (!m_pEffect) {
        // return effectElement;
    // }

    // QDomElement metaKnobElement = doc->createElement(EffectXml::EffectMetaParameter);
    // XmlParse::addElement(*doc, effectElement,
    //                      EffectXml::EffectMetaParameter,
    //                      QString::number(m_pControlMetaParameter->get()));
    // EffectManifestPointer pManifest = m_pEffect->getManifest();
    // XmlParse::addElement(*doc, effectElement,
    //                      EffectXml::EffectId, pManifest->id());
    // XmlParse::addElement(*doc, effectElement,
    //                      EffectXml::EffectVersion, pManifest->version());

    // QDomElement parametersElement = doc->createElement(EffectXml::ParametersRoot);

    // for (const auto& pParameter : m_knobParameterSlots) {
    //     QDomElement parameterElement = pParameter->toXml(doc);
    //     if (!parameterElement.hasChildNodes()) {
    //         continue;
    //     }
    //     EffectManifestParameterPointer manifest = pParameter->getManifest();
    //     if (!manifest) {
    //         continue;
    //     }
    //     XmlParse::addElement(*doc, parameterElement,
    //                          EffectXml::ParameterId,
    //                          manifest->id());
    //     parametersElement.appendChild(parameterElement);
    // }
    // for (const auto& pParameter : m_buttonParameterSlots) {
    //     QDomElement parameterElement = pParameter->toXml(doc);
    //     if (!parameterElement.hasChildNodes()) {
    //         continue;
    //     }
    //     EffectManifestParameterPointer manifest = pParameter->getManifest();
    //     if (!manifest) {
    //         continue;
    //     }
    //     XmlParse::addElement(*doc, parameterElement,
    //                          EffectXml::ParameterId,
    //                          pParameter->getManifest()->id());
    //     parametersElement.appendChild(parameterElement);
    // }

    // effectElement.appendChild(parametersElement);

    return effectElement;
}

void EffectSlot::loadEffectSlotFromXml(const QDomElement& effectElement) {
    // if (!m_pEffect) {
    //     return;
    // }

    // if (!effectElement.hasChildNodes()) {
    //     return;
    // }

    // QDomElement effectIdElement = XmlParse::selectElement(effectElement,
    //                                                       EffectXml::EffectId);
    // if (m_pEffect->getManifest()->id() != effectIdElement.text()) {
    //     qWarning() << "EffectSlot::loadEffectSlotFromXml"
    //                << "effect ID in XML does not match presently loaded effect, ignoring.";
    //     return;
    // }

    // m_pControlMetaParameter->set(XmlParse::selectNodeDouble(
    //         effectElement, EffectXml::EffectMetaParameter));
    // QDomElement parametersElement = XmlParse::selectElement(
    //         effectElement, EffectXml::ParametersRoot);
    // if (!parametersElement.hasChildNodes()) {
    //     return;
    // }

    // QMap<QString, EffectParameterSlotBasePointer> parametersById;
    // for (const auto& pParameter : m_knobParameterSlots) {
    //     EffectManifestParameterPointer manifest = pParameter->getManifest();
    //     if (manifest) {
    //         parametersById.insert(manifest->id(), pParameter);
    //     }
    // }
    // for (const auto& pParameter : m_buttonParameterSlots) {
    //     EffectManifestParameterPointer manifest = pParameter->getManifest();
    //     if (manifest) {
    //         parametersById.insert(manifest->id(), pParameter);
    //     }
    // }

    // QDomNodeList parametersNodeList = parametersElement.childNodes();
    // for (int i = 0; i < parametersNodeList.size(); ++i) {
    //     QDomNode parameterNode = parametersNodeList.at(i);
    //     if (parameterNode.isElement()) {
    //         const QString id = XmlParse::selectNodeQString(parameterNode,
    //                                                        EffectXml::ParameterId);
    //         EffectParameterSlotBasePointer pParameterSlot = parametersById.value(id);
    //         if (pParameterSlot != nullptr) {
    //             pParameterSlot->loadParameterSlotFromXml(parameterNode.toElement());
    //         }
    //     }
    // }
}
