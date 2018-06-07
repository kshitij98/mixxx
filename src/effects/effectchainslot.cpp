#include "effects/effectchainslot.h"

#include "effects/effectrack.h"
#include "effects/effectxmlelements.h"
#include "effects/effectslot.h"
#include "engine/effects/engineeffectchain.h"
#include "control/controlpotmeter.h"
#include "control/controlpushbutton.h"
#include "control/controlencoder.h"
#include "mixer/playermanager.h"
#include "util/math.h"
#include "util/xml.h"

// START EFFECTCHAIN
#include "engine/engine.h"
#include "effects/effectchainmanager.h"
#include "effects/effectsmanager.h"
#include "effects/effectprocessor.h"
// #include "effects/effectxmlelements.h"
// #include "engine/effects/engineeffectchain.h"
#include "engine/effects/engineeffectrack.h"
#include "engine/effects/message.h"
#include "util/defs.h"
#include "util/sample.h"
// #include "util/xml.h"
// END EFFECTCHAIN


EffectChainSlot::EffectChainSlot(EffectRack* pRack, const QString& group,
                                 unsigned int iChainNumber,
                                 EffectsManager* pEffectsManager,
                                 const QString& id)
        : m_iChainSlotNumber(iChainNumber),
          // The control group names are 1-indexed while internally everything
          // is 0-indexed.
          m_group(group),
          m_pEffectRack(pRack),
          m_pEffectsManager(pEffectsManager),
          m_bEnabled(true),
          m_id(id),
          m_name(""),
          m_mixMode(EffectChainMixMode::DrySlashWet),
          m_dMix(0),
          m_pEngineEffectChain(nullptr),
          m_bAddedToEngine(false) {
    qDebug() << "EffectChainSlot::EffectChainSlot " << pRack << ' ' << group << ' ' << iChainNumber;

    m_pControlClear = new ControlPushButton(ConfigKey(m_group, "clear"));
    connect(m_pControlClear, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlClear(double)));

    m_pControlNumEffects = new ControlObject(ConfigKey(m_group, "num_effects"));
    m_pControlNumEffects->setReadOnly();

    m_pControlNumEffectSlots = new ControlObject(ConfigKey(m_group, "num_effectslots"));
    m_pControlNumEffectSlots->setReadOnly();

    m_pControlChainLoaded = new ControlObject(ConfigKey(m_group, "loaded"));
    m_pControlChainLoaded->setReadOnly();

    m_pControlChainEnabled = new ControlPushButton(ConfigKey(m_group, "enabled"));
    m_pControlChainEnabled->setButtonMode(ControlPushButton::POWERWINDOW);
    // Default to enabled. The skin might not show these buttons.
    m_pControlChainEnabled->setDefaultValue(true);
    m_pControlChainEnabled->set(true);
    connect(m_pControlChainEnabled, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlChainEnabled(double)));

    m_pControlChainMix = new ControlPotmeter(ConfigKey(m_group, "mix"), 0.0, 1.0,
                                             false, true, false, true, 1.0);
    connect(m_pControlChainMix, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlChainMix(double)));

    m_pControlChainSuperParameter = new ControlPotmeter(ConfigKey(m_group, "super1"), 0.0, 1.0);
    connect(m_pControlChainSuperParameter, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlChainSuperParameter(double)));
    m_pControlChainSuperParameter->set(0.0);
    m_pControlChainSuperParameter->setDefaultValue(0.0);

    m_pControlChainMixMode = new ControlPushButton(ConfigKey(m_group, "mix_mode"));
    m_pControlChainMixMode->setButtonMode(ControlPushButton::TOGGLE);
    m_pControlChainMixMode->setStates(static_cast<double>(EffectChainMixMode::NumMixModes));
    connect(m_pControlChainMixMode, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlChainMixMode(double)));

    m_pControlChainNextPreset = new ControlPushButton(ConfigKey(m_group, "next_chain"));
    connect(m_pControlChainNextPreset, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlChainNextPreset(double)));

    m_pControlChainPrevPreset = new ControlPushButton(ConfigKey(m_group, "prev_chain"));
    connect(m_pControlChainPrevPreset, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlChainPrevPreset(double)));

    // Ignoring no-ops is important since this is for +/- tickers.
    m_pControlChainSelector = new ControlEncoder(ConfigKey(m_group, "chain_selector"), false);
    connect(m_pControlChainSelector, SIGNAL(valueChanged(double)),
            this, SLOT(slotControlChainSelector(double)));

    connect(&m_channelStatusMapper, SIGNAL(mapped(const QString&)),
            this, SLOT(slotChannelStatusChanged(const QString&)));

    // ControlObjects for skin <-> controller mapping interaction.
    // Refer to comment in header for full explanation.
    m_pControlChainShowFocus = new ControlPushButton(
                                   ConfigKey(m_group, "show_focus"));
    m_pControlChainShowFocus->setButtonMode(ControlPushButton::TOGGLE);

    m_pControlChainShowParameters = new ControlPushButton(
                                        ConfigKey(m_group, "show_parameters"),
                                        true);
    m_pControlChainShowParameters->setButtonMode(ControlPushButton::TOGGLE);

    m_pControlChainFocusedEffect = new ControlPushButton(
                                       ConfigKey(m_group, "focused_effect"),
                                       true);
    m_pControlChainFocusedEffect->setButtonMode(ControlPushButton::TOGGLE);

    addToEngine(m_pEffectRack->getEngineEffectRack(), m_iChainSlotNumber);
}

EffectChainSlot::~EffectChainSlot() {
    //qDebug() << debugString() << "destroyed";
    clear();
    delete m_pControlClear;
    delete m_pControlNumEffects;
    delete m_pControlNumEffectSlots;
    delete m_pControlChainLoaded;
    delete m_pControlChainEnabled;
    delete m_pControlChainMix;
    delete m_pControlChainSuperParameter;
    delete m_pControlChainMixMode;
    delete m_pControlChainPrevPreset;
    delete m_pControlChainNextPreset;
    delete m_pControlChainSelector;
    delete m_pControlChainShowFocus;
    delete m_pControlChainShowParameters;
    delete m_pControlChainFocusedEffect;

    for (QMap<QString, ChannelInfo*>::iterator it = m_channelInfoByName.begin();
         it != m_channelInfoByName.end();) {
        delete it.value();
        it = m_channelInfoByName.erase(it);
    }

    m_slots.clear();
    m_pEffectChain.clear();
    removeFromEngine(m_pEffectRack->getEngineEffectRack(), m_iChainSlotNumber);
}

// START EFFECTCHAIN
void EffectChainSlot::addToEngine(EngineEffectRack* pRack, int iIndex) {
    qDebug() << "Adding effect chain to engine" << m_id << ' ' << iIndex;
    m_pEngineEffectChain = new EngineEffectChain(m_id,
        m_pEffectsManager->registeredInputChannels(),
        m_pEffectsManager->registeredOutputChannels());
    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::ADD_CHAIN_TO_RACK;
    pRequest->pTargetRack = pRack;
    pRequest->AddChainToRack.pChain = m_pEngineEffectChain;
    pRequest->AddChainToRack.iIndex = iIndex;
    m_pEffectsManager->writeRequest(pRequest);
    m_bAddedToEngine = true;

    // Add all effects.
    for (int i = 0; i < m_effects.size(); ++i) {
        // Add the effect to the engine.
        EffectPointer pEffect = m_effects[i];
        if (pEffect) {
            pEffect->addToEngine(m_pEngineEffectChain, i, m_enabledInputChannels);
        }
    }
}

void EffectChainSlot::removeFromEngine(EngineEffectRack* pRack, int iIndex) {
    if (!m_bAddedToEngine) {
        return;
    }

    // Order doesn't matter when removing.
    for (int i = 0; i < m_effects.size(); ++i) {
        EffectPointer pEffect = m_effects[i];
        if (pEffect) {
            pEffect->removeFromEngine(m_pEngineEffectChain, i);
        }
    }

    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::REMOVE_CHAIN_FROM_RACK;
    pRequest->pTargetRack = pRack;
    pRequest->RemoveChainFromRack.pChain = m_pEngineEffectChain;
    pRequest->RemoveChainFromRack.iIndex = iIndex;
    m_pEffectsManager->writeRequest(pRequest);
    m_bAddedToEngine = false;

    m_pEngineEffectChain = nullptr;
}

void EffectChainSlot::updateEngineState() {
    if (!m_bAddedToEngine) {
        return;
    }
    // Update chain parameters in the engine.
    sendParameterUpdate();
    for (int i = 0; i < m_effects.size(); ++i) {
        EffectPointer pEffect = m_effects[i];
        if (pEffect) {
            // Update effect parameters in the engine.
            pEffect->updateEngineState();
        }
    }
}

// static
// NOTE(Kshitij) : Changed EffectChain::clone to EffectChainSlot::clone function
EffectChainPointer EffectChainSlot::clone(EffectChainPointer pChain) {
    if (!pChain) {
        return EffectChainPointer();
    }

    EffectChain* pClone = new EffectChain(
        pChain->m_pEffectsManager, pChain->id(), pChain);
    pClone->setName(pChain->name());
    // Do not set the state of the chain because that information belongs
    // to the EffectChainSlot. Leave that to EffectChainSlot::loadEffectChain.
    for (const auto& pEffect : pChain->effects()) {
        EffectPointer pClonedEffect;
        if (pEffect == nullptr) {
            // Insert empty effect to preserve chain order
            pClonedEffect = EffectPointer();
        } else {
            pClonedEffect = pChain->m_pEffectsManager->instantiateEffect(
                    pEffect->getManifest()->id());
        }
        pClone->addEffect(pClonedEffect);
    }
    return EffectChainPointer(pClone);
}
// EffectChainSlotPointer EffectChainSlot::clone(EffectChainSlotPointer pChainSlot) {
//     if (!pChainSlot) {
//         return EffectChainSlotPointer();
//     }

//     EffectChain* pClone = new EffectChainSlot(
//         pChainSlot->m_pEffectsManager, pChainSlot->id(), pChainSlot);
//     pClone->setName(pChain->name());
//     // Do not set the state of the chain because that information belongs
//     // to the EffectChainSlot. Leave that to EffectChainSlot::loadEffectChain.
//     for (const auto& pEffect : pChain->effects()) {
//         EffectPointer pClonedEffect;
//         if (pEffect == nullptr) {
//             // Insert empty effect to preserve chain order
//             pClonedEffect = EffectPointer();
//         } else {
//             pClonedEffect = pChain->m_pEffectsManager->instantiateEffect(
//                     pEffect->getManifest()->id());
//         }
//         pClone->addEffect(pClonedEffect);
//     }
//     return EffectChainPointer(pClone);
// }

bool EffectChainSlot::isEmpty() const {
    return (m_id == QString());
}

// EffectRack* EffectChainSlot::rack() const {
//     return m_pEffectRack;
// }

EffectChainPointer EffectChainSlot::prototype() const {
    return m_pPrototype;
}

// const QString& EffectChainSlot::id() const {
//     return m_id;
// }

const QString& EffectChainSlot::name() const {
    return m_name;
}

void EffectChainSlot::setName(const QString& name) {
    m_name = name;
    // emit(nameChanged(name));
    slotChainNameChanged(name);
}

QString EffectChainSlot::description() const {
    return m_description;
}

void EffectChainSlot::setDescription(const QString& description) {
    m_description = description;
    // emit(descriptionChanged(description));
}

bool EffectChainSlot::enabled() const {
    return m_bEnabled;
}

void EffectChainSlot::setEnabled(bool enabled) {
    m_bEnabled = enabled;
    sendParameterUpdate();
    // emit(enabledChanged(enabled));
    slotChainEnabledChanged(enabled);
}

void EffectChainSlot::enableForInputChannel(const ChannelHandleAndGroup& handle_group) {
    // TODO(Be): remove m_enabledChannels from this class and move this logic
    // to EffectChainSlot
    bool bWasAlreadyEnabled = m_enabledInputChannels.contains(handle_group);
    if (!bWasAlreadyEnabled) {
        m_enabledInputChannels.insert(handle_group);
    }

    // The allocation of EffectStates below may be expensive, so avoid it if
    // not needed.
    if (!m_bAddedToEngine || bWasAlreadyEnabled) {
        return;
    }

    EffectsRequest* request = new EffectsRequest();
    request->type = EffectsRequest::ENABLE_EFFECT_CHAIN_FOR_INPUT_CHANNEL;
    request->pTargetChain = m_pEngineEffectChain;
    request->EnableInputChannelForChain.pChannelHandle = &handle_group.handle();

    // Allocate EffectStates here in the main thread to avoid allocating
    // memory in the realtime audio callback thread. Pointers to the
    // EffectStates are passed to the EffectRequest and the EffectProcessorImpls
    // store the pointers. The containers of EffectState* pointers get deleted
    // by ~EffectsRequest, but the EffectStates are managed by EffectProcessorImpl.
    auto pEffectStatesMapArray = new EffectStatesMapArray;

    //TODO: get actual configuration of engine
    const mixxx::EngineParameters bufferParameters(
          mixxx::AudioSignal::SampleRate(96000),
          MAX_BUFFER_LEN / mixxx::kEngineChannelCount);

    for (int i = 0; i < m_effects.size(); ++i) {
        auto& statesMap = (*pEffectStatesMapArray)[i];
        if (m_effects[i] != nullptr) {
            for (const auto& outputChannel : m_pEffectsManager->registeredOutputChannels()) {
                if (kEffectDebugOutput) {
                    qDebug() << debugString() << "EffectChain::enableForInputChannel creating EffectState for input" << handle_group << "output" << outputChannel;
                }
                statesMap.insert(outputChannel.handle(),
                        m_effects[i]->createState(bufferParameters));
            }
        } else {
            for (EffectState* pState : statesMap) {
                if (pState != nullptr) {
                    delete pState;
                }
            }
            statesMap.clear();
        }
    }
    request->EnableInputChannelForChain.pEffectStatesMapArray = pEffectStatesMapArray;

    m_pEffectsManager->writeRequest(request);
    // emit(channelStatusChanged(handle_group.name(), true));
    slotChainChannelStatusChanged(handle_group.name(), true);
}

bool EffectChainSlot::enabledForChannel(const ChannelHandleAndGroup& handle_group) const {
    return m_enabledInputChannels.contains(handle_group);
}

void EffectChainSlot::disableForInputChannel(const ChannelHandleAndGroup& handle_group) {
    if (m_enabledInputChannels.remove(handle_group)) {
        if (!m_bAddedToEngine) {
            return;
        }
        EffectsRequest* request = new EffectsRequest();
        request->type = EffectsRequest::DISABLE_EFFECT_CHAIN_FOR_INPUT_CHANNEL;
        request->pTargetChain = m_pEngineEffectChain;
        request->DisableInputChannelForChain.pChannelHandle = &handle_group.handle();
        m_pEffectsManager->writeRequest(request);

        // emit(channelStatusChanged(handle_group.name(), false));
        slotChainChannelStatusChanged(handle_group.name(), false);
    }
}

double EffectChainSlot::mix() const {
    return m_dMix;
}

void EffectChainSlot::setMix(const double& dMix) {
    m_dMix = dMix;
    sendParameterUpdate();
    // emit(mixChanged(dMix));
    slotChainMixChanged(dMix);
}

EffectChainMixMode EffectChainSlot::mixMode() const {
    return m_mixMode;
}

void EffectChainSlot::setMixMode(EffectChainMixMode mixMode) {
    m_mixMode = mixMode;
    sendParameterUpdate();
    // emit(mixModeChanged(mixMode));
    slotChainMixModeChanged(mixMode);
}

void EffectChainSlot::addEffect(EffectPointer pEffect) {
    //qDebug() << debugString() << "addEffect" << pEffect;
    if (!pEffect) {
        // Insert empty effects to preserve chain order
        // when loading chains with empty effects
        m_effects.append(pEffect);
        return;
    }

    VERIFY_OR_DEBUG_ASSERT(!m_effects.contains(pEffect)) {
        return;
    }

    m_effects.append(pEffect);
    if (m_bAddedToEngine) {
        pEffect->addToEngine(m_pEngineEffectChain, m_effects.size() - 1, m_enabledInputChannels);
    }
    // emit(effectChanged(m_effects.size() - 1));
    slotChainEffectChanged(m_effects.size() - 1);
}

void EffectChainSlot::replaceEffect(unsigned int effectSlotNumber,
                                EffectPointer pEffect) {
    qDebug() << debugString() << "replaceEffect" << effectSlotNumber << pEffect;
    while (effectSlotNumber >= static_cast<unsigned int>(m_effects.size())) {
        if (pEffect.isNull()) {
            return;
        }
        m_effects.append(EffectPointer());
    }

    qDebug() << "Added to engine? = " << m_bAddedToEngine;

    EffectPointer pOldEffect = m_effects[effectSlotNumber];
    if (!pOldEffect.isNull()) {
        if (m_bAddedToEngine) {
            qDebug() << "Removing " << pOldEffect->getManifest()->id() << " loaded at " << effectSlotNumber;
            pOldEffect->removeFromEngine(m_pEngineEffectChain, effectSlotNumber);
        }
    }

    m_effects.replace(effectSlotNumber, pEffect);
    if (!pEffect.isNull()) {
        if (m_bAddedToEngine) {
            qDebug() << "Adding " << pEffect->getManifest()->id() << " at " << effectSlotNumber;
            qDebug() << "INPUT CHANNELS: ";
            for (auto channel : m_enabledInputChannels) {
                qDebug() << channel;
            }
            qDebug() << "END INPUT CHANNELS";
            pEffect->addToEngine(m_pEngineEffectChain, effectSlotNumber, m_enabledInputChannels);
        }
    }

    // emit(effectChanged(effectSlotNumber));
    slotChainEffectChanged(effectSlotNumber);
}

void EffectChainSlot::removeEffect(unsigned int effectSlotNumber) {
    replaceEffect(effectSlotNumber, EffectPointer());
}

void EffectChainSlot::refreshAllEffects() {
    for (int i = 0; i < m_effects.size(); ++i) {
        // emit(effectChanged(i));
        slotChainEffectChanged(i);        
    }
}

unsigned int EffectChainSlot::numEffects() const {
    return m_effects.size();
}

const QList<EffectPointer>& EffectChainSlot::effects() const {
    return m_effects;
}

EngineEffectChain* EffectChainSlot::getEngineEffectChain() {
    return m_pEngineEffectChain;
}

void EffectChainSlot::sendParameterUpdate() {
    qDebug() << "ADDEDD TO ENGINE = " << m_bAddedToEngine;
    if (!m_bAddedToEngine) {
        return;
    }
    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::SET_EFFECT_CHAIN_PARAMETERS;
    pRequest->pTargetChain = m_pEngineEffectChain;
    pRequest->SetEffectChainParameters.enabled = m_bEnabled;
    pRequest->SetEffectChainParameters.mix_mode = m_mixMode;
    pRequest->SetEffectChainParameters.mix = m_dMix;
    m_pEffectsManager->writeRequest(pRequest);
}

// static
EffectChainPointer EffectChainSlot::createFromXml(EffectsManager* pEffectsManager,
                                        const QDomElement& element) {
    if (!element.hasChildNodes()) {
        // An empty element <EffectChain/> is treated as an ejected Chain (null)
        return EffectChainPointer();
    }

    QString id = XmlParse::selectNodeQString(element,
                                             EffectXml::ChainId);
    QString name = XmlParse::selectNodeQString(element,
                                               EffectXml::ChainName);
    QString description = XmlParse::selectNodeQString(element,
                                                      EffectXml::ChainDescription);
    QString mixModeStr = XmlParse::selectNodeQString(element,
                                                           EffectXml::ChainMixMode);

    EffectChainPointer pChain(new EffectChain(pEffectsManager, id));
    pChain->setName(name);
    pChain->setDescription(description);
    EffectChainMixMode mixMode = mixModeFromString(mixModeStr);
    if (mixMode != EffectChainMixMode::NumMixModes) {
        pChain->setMixMode(mixMode);
    }

    QDomElement effects = XmlParse::selectElement(element, EffectXml::EffectsRoot);
    QDomNodeList effectChildren = effects.childNodes();

    for (int i = 0; i < effectChildren.count(); ++i) {
        QDomNode effect = effectChildren.at(i);
        if (effect.isElement()) {
            EffectPointer pEffect = Effect::createFromXml(
                pEffectsManager, effect.toElement());
            pChain->addEffect(pEffect);
        }
    }

    return pChain;
}
// END EFFECTCHAIN


QString EffectChainSlot::id() const {
    return m_id;
    // if (m_pEffectChain)
    //     return m_pEffectChain->id();
    // return "";
}

double EffectChainSlot::getSuperParameter() const {
    return m_pControlChainSuperParameter->get();
}

void EffectChainSlot::setSuperParameter(double value, bool force) {
    m_pControlChainSuperParameter->set(value);
    slotControlChainSuperParameter(value, force);
}

void EffectChainSlot::setSuperParameterDefaultValue(double value) {
    m_pControlChainSuperParameter->setDefaultValue(value);
}

void EffectChainSlot::slotChainNameChanged(const QString&) {
    emit(updated());
}

void EffectChainSlot::slotChainEnabledChanged(bool bEnabled) {
    m_pControlChainEnabled->set(bEnabled);
    emit(updated());
}

void EffectChainSlot::slotChainMixChanged(double mix) {
    m_pControlChainMix->set(mix);
    emit(updated());
}

void EffectChainSlot::slotChainMixModeChanged(EffectChainMixMode mixMode) {
    m_pControlChainMixMode->set(static_cast<double>(mixMode));
    emit(updated());
}

void EffectChainSlot::slotChainChannelStatusChanged(const QString& group,
                                                    bool enabled) {
    ChannelInfo* pInfo = m_channelInfoByName.value(group, NULL);
    if (pInfo != NULL && pInfo->pEnabled != NULL) {
        pInfo->pEnabled->set(enabled);
        emit(updated());
    }
}

void EffectChainSlot::slotChainEffectChanged(unsigned int effectSlotNumber,
                                             bool shouldEmit) {
    qDebug() << debugString() << "slotChainEffectChanged" << effectSlotNumber;

    qDebug() << "START-----";
    for (auto &channel : m_enabledInputChannels) {
        qDebug() << channel;
    }
    qDebug() << "END-----";
    // NOTE(Kshitij) : m_pEffectChain dependancy
    // if (m_pEffectChain) {
    //     const QList<EffectPointer> effects = m_pEffectChain->effects();
    //     EffectSlotPointer pSlot;
    //     EffectPointer pEffect;

    //     if (effects.size() > m_slots.size()) {
    //         qWarning() << debugString() << "has too few slots for effect";
    //     }

    //     if (effectSlotNumber < (unsigned) m_slots.size()) {
    //         pSlot = m_slots.at(effectSlotNumber);
    //     }
    //     if (effectSlotNumber < (unsigned) effects.size()) {
    //         pEffect = effects.at(effectSlotNumber);
    //     }
    //     if (pSlot != nullptr) {
    //         pSlot->loadEffect(pEffect, m_pEffectRack->isAdoptMetaknobValueEnabled());
    //     }

    //     m_pControlNumEffects->forceSet(math_min(
    //             static_cast<unsigned int>(m_slots.size()),
    //             m_pEffectChain->numEffects()));

    //     if (shouldEmit) {
    //         emit(updated());
    //     }
    // }
    // IMPORTANT(Kshitij) : Removed if for testing
    // if (!isEmpty()) {
        // NOTE(Kshitij) : Check a clone is being made in the following line:
        // const QList<EffectPointer> effects = m_pEffectChain->effects();
        EffectSlotPointer pSlot;
        EffectPointer pEffect;

        if (m_effects.size() > m_slots.size()) {
            qWarning() << debugString() << "has too few slots for effect";
        }

        if (effectSlotNumber < (unsigned) m_slots.size()) {
            pSlot = m_slots.at(effectSlotNumber);
        }
        if (effectSlotNumber < (unsigned) m_effects.size()) {
            pEffect = m_effects.at(effectSlotNumber);
        }
        if (pSlot != nullptr) {
            pSlot->loadEffect(pEffect, m_pEffectRack->isAdoptMetaknobValueEnabled());
        }

        m_pControlNumEffects->forceSet(math_min(
                static_cast<unsigned int>(m_slots.size()),
                numEffects()));


        qDebug() << debugString() << "should Emit for " << m_id << " = " << shouldEmit;
        if (shouldEmit) {
            emit(updated());
        }
    // }
}

void EffectChainSlot::loadEffectChainToSlot(EffectChainPointer pEffectChain) {
    qDebug() << debugString() << "loadEffectChainToSlot" << (pEffectChain ? pEffectChain->id() : "(null)");
    clear();

    // if (pEffectChain) {
        // m_pEffectChain = pEffectChain;

        // connect(m_pEffectChain.data(), SIGNAL(effectChanged(unsigned int)),
        //         this, SLOT(slotChainEffectChanged(unsigned int)));
        // connect(m_pEffectChain.data(), SIGNAL(nameChanged(const QString&)),
        //         this, SLOT(slotChainNameChanged(const QString&)));
        // connect(m_pEffectChain.data(), SIGNAL(enabledChanged(bool)),
        //         this, SLOT(slotChainEnabledChanged(bool)));
        // connect(m_pEffectChain.data(), SIGNAL(mixChanged(double)),
        //         this, SLOT(slotChainMixChanged(double)));
        // connect(m_pEffectChain.data(), SIGNAL(mixModeChanged(EffectChainMixMode)),
        //         this, SLOT(slotChainMixModeChanged(EffectChainMixMode)));
        // connect(m_pEffectChain.data(), SIGNAL(channelStatusChanged(const QString&, bool)),
        //         this, SLOT(slotChainChannelStatusChanged(const QString&, bool)));

        m_pControlChainLoaded->forceSet(true);

        // NOTE(Kshitij) : Updated the following function
        // m_pControlChainMixMode->set(
        //         static_cast<double>(m_pEffectChain->mixMode()));
        m_pControlChainMixMode->set(
                static_cast<double>(mixMode()));

        // Mix and enabled channels are persistent properties of the chain slot,
        // not of the chain. Propagate the current settings to the chain.

        // NOTE(Kshitij) : Updated the functions
        // m_pEffectChain->setMix(m_pControlChainMix->get());
        // m_pEffectChain->setEnabled(m_pControlChainEnabled->get() > 0.0);
        setMix(m_pControlChainMix->get());
        setEnabled(m_pControlChainEnabled->get() > 0.0);

        // Don't emit because we will below.
        for (int i = 0; i < m_slots.size(); ++i) {
            slotChainEffectChanged(i, false);
        }
    // }

    // NOTE(Kshitij) : Not being used so redundant
    // emit(effectChainLoaded(pEffectChain));
    emit(updated());
}

void EffectChainSlot::updateRoutingSwitches() {
    // NOTE(Kshitij) : Updated the assert line to return statement
    // VERIFY_OR_DEBUG_ASSERT(m_pEffectChain) {
    //     return;
    // }

    VERIFY_OR_DEBUG_ASSERT(false) {
        return;
    }
    
    // NOTE(Kshitij) : Removed m_pEffectChain dependancy
    // if (!m_pEffectChain) {
    //     return;
    // }
    // for (const ChannelInfo* pChannelInfo : m_channelInfoByName) {
    //     if (pChannelInfo->pEnabled->toBool()) {
    //         m_pEffectChain->enableForInputChannel(pChannelInfo->handle_group);
    //     } else {
    //         m_pEffectChain->disableForInputChannel(pChannelInfo->handle_group);
    //     }
    // }
    qDebug() << "updatingRoutingSwitches()";
    // if (isEmpty()) {
    //     return;
    // }
    for (const ChannelInfo* pChannelInfo : m_channelInfoByName) {
        if (pChannelInfo->pEnabled->toBool()) {
            enableForInputChannel(pChannelInfo->handle_group);
        } else {
            disableForInputChannel(pChannelInfo->handle_group);
        }
    }
}

EffectChainPointer EffectChainSlot::getEffectChain() const {
    // TODO(Kshitij) : Remove this function
    return m_pEffectChain;
}

// TODO(Kshitij) : Return EffectChainPreset instead of EffectChain
// IMPORTANT(Kshitij) : Check what to return and keep the code working
// EffectChainPointer EffectChainSlot::getOrCreateEffectChain(
//         EffectsManager* pEffectsManager) {
//     // if (!m_pEffectChain) {
//     //     EffectChainPointer pEffectChain(
//     //             new EffectChain(pEffectsManager, QString()));
//     //     //: Name for an empty effect chain, that is created after eject
//     //     pEffectChain->setName(tr("Empty Chain"));
//     //     loadEffectChainToSlot(pEffectChain);
//     //     pEffectChain->addToEngine(m_pEffectRack->getEngineEffectRack(), m_iChainSlotNumber);
//     //     pEffectChain->updateEngineState();
//     //     updateRoutingSwitches();
//     // }
//     // return m_pEffectChain;
//     if (isEmpty()) {
//         // EffectChainPointer pEffectChain(
//         //         new EffectChain(pEffectsManager, QString()));
//         // IMPORTANT(Kshitij) : Id is still null, isEmpty() would still return true. Update isEmpty function and its name or update the id here as well.
//         //: Name for an empty effect chain, that is created after eject
//         setName(tr("Empty Chain"));
//         loadEffectChainToSlot();
//         // addToEngine(m_pEffectRack->getEngineEffectRack(), m_iChainSlotNumber);
//         updateEngineState();
//         updateRoutingSwitches();
//     }
//     return m_pEffectChain;
// }

void EffectChainSlot::clear() {
    // Stop listening to signals from any loaded effect
    // NOTE(Kshitij) : Removed m_pEffectChain usage
    // if (m_pEffectChain) {
    //     m_pEffectChain->removeFromEngine(m_pEffectRack->getEngineEffectRack(),
    //                                      m_iChainSlotNumber);
    //     for (EffectSlotPointer pSlot : m_slots) {
    //         pSlot->clear();
    //     }
    //     m_pEffectChain->disconnect(this);
    //     m_pEffectChain.clear();
    // }
    m_pControlNumEffects->forceSet(0.0);
    m_pControlChainLoaded->forceSet(0.0);
    m_pControlChainMixMode->set(
            static_cast<double>(EffectChainMixMode::DrySlashWet));
    emit(updated());
}

unsigned int EffectChainSlot::numSlots() const {
    qDebug() << debugString() << "numSlots";
    return m_slots.size();
}

EffectSlotPointer EffectChainSlot::addEffectSlot(const QString& group) {
    qDebug() << debugString() << "addEffectSlot" << group;

    EffectSlot* pEffectSlot = new EffectSlot(group, m_iChainSlotNumber,
                                             m_slots.size());
    // Rebroadcast effectLoaded signals
    connect(pEffectSlot, SIGNAL(effectLoaded(EffectPointer, unsigned int)),
            this, SLOT(slotEffectLoaded(EffectPointer, unsigned int)));
    connect(pEffectSlot, SIGNAL(clearEffect(unsigned int)),
            this, SLOT(slotClearEffect(unsigned int)));
    connect(pEffectSlot, SIGNAL(nextEffect(unsigned int, unsigned int, EffectPointer)),
            this, SIGNAL(nextEffect(unsigned int, unsigned int, EffectPointer)));
    connect(pEffectSlot, SIGNAL(prevEffect(unsigned int, unsigned int, EffectPointer)),
            this, SIGNAL(prevEffect(unsigned int, unsigned int, EffectPointer)));

    EffectSlotPointer pSlot(pEffectSlot);
    m_slots.append(pSlot);
    int numEffectSlots = m_pControlNumEffectSlots->get() + 1;
    m_pControlNumEffectSlots->forceSet(numEffectSlots);
    m_pControlChainFocusedEffect->setStates(numEffectSlots);
    return pSlot;
}

void EffectChainSlot::registerInputChannel(const ChannelHandleAndGroup& handle_group) {
    VERIFY_OR_DEBUG_ASSERT(!m_channelInfoByName.contains(handle_group.name())) {
        return;
    }

    qDebug() << "registerInputChannel" << handle_group;

    double initialValue = 0.0;
    int deckNumber;
    if (PlayerManager::isDeckGroup(handle_group.name(), &deckNumber) &&
        (m_iChainSlotNumber + 1) == (unsigned) deckNumber) {
        initialValue = 1.0;
    }
    ControlPushButton* pEnableControl = new ControlPushButton(
            ConfigKey(m_group, QString("group_%1_enable").arg(handle_group.name())),
            true, initialValue);
    pEnableControl->setButtonMode(ControlPushButton::POWERWINDOW);

    ChannelInfo* pInfo = new ChannelInfo(handle_group, pEnableControl);
    m_channelInfoByName[handle_group.name()] = pInfo;
    m_channelStatusMapper.setMapping(pEnableControl, handle_group.name());
    connect(pEnableControl, SIGNAL(valueChanged(double)),
            &m_channelStatusMapper, SLOT(map()));

    // NOTE(Kshitij) : Already enabled input channels should be added
    // IMPORTANT(Kshitij) : Is this enough to init?
    if (pInfo->pEnabled->toBool()) {
        enableForInputChannel(pInfo->handle_group);
    } else {
        disableForInputChannel(pInfo->handle_group);
    }
}

void EffectChainSlot::slotEffectLoaded(EffectPointer pEffect, unsigned int slotNumber) {
    // const int is a safe read... don't bother locking
    qDebug() << debugString()  << slotNumber;
    emit(effectLoaded(pEffect, m_iChainSlotNumber, slotNumber));
}

void EffectChainSlot::slotClearEffect(unsigned int iEffectSlotNumber) {
    // NOTE(Kshitij) : Removed the reference to m_pEffectChain
    // if (m_pEffectChain) {
    //     m_pEffectChain->removeEffect(iEffectSlotNumber);
    // }
    // if (m_pEffectChain) {
    removeEffect(iEffectSlotNumber);
    // }
}

EffectSlotPointer EffectChainSlot::getEffectSlot(unsigned int slotNumber) {
    //qDebug() << debugString() << "getEffectSlot" << slotNumber;
    if (slotNumber >= static_cast<unsigned int>(m_slots.size())) {
        qWarning() << "WARNING: slotNumber out of range";
        return EffectSlotPointer();
    }
    return m_slots[slotNumber];
}

void EffectChainSlot::slotControlClear(double v) {
    if (v > 0) {
        clear();
    }
}

void EffectChainSlot::slotControlChainEnabled(double v) {
    qDebug() << debugString() << "slotControlChainEnabled" << v;
    // NOTE(Kshitij) : Removed the reference to m_pEffectChain
    // if (m_pEffectChain) {
    //     m_pEffectChain->setEnabled(v > 0);
    // }
    // if (m_pEffectChain) {
    setEnabled(v > 0);
    // }
}

void EffectChainSlot::slotControlChainMix(double v) {
    qDebug() << debugString() << "slotControlChainMix" << v;

    // Clamp to [0.0, 1.0]
    if (v < 0.0 || v > 1.0) {
        qWarning() << debugString() << "value out of limits";
        v = math_clamp(v, 0.0, 1.0);
        m_pControlChainMix->set(v);
    }
    // NOTE(Kshitij) : Removed the reference to m_pEffectChain
    // if (m_pEffectChain) {
    //     m_pEffectChain->setMix(v);
    // }
    // if (m_pEffectChain) {
    setMix(v);
    // }
}

void EffectChainSlot::slotControlChainSuperParameter(double v, bool force) {
    qDebug() << debugString() << "slotControlChainSuperParameter" << v;

    // Clamp to [0.0, 1.0]
    if (v < 0.0 || v > 1.0) {
        qWarning() << debugString() << "value out of limits";
        v = math_clamp(v, 0.0, 1.0);
        m_pControlChainSuperParameter->set(v);
    }
    for (const auto& pSlot : m_slots) {
        pSlot->setMetaParameter(v, force);
    }
}

void EffectChainSlot::slotControlChainMixMode(double v) {
    // Intermediate cast to integer is needed for VC++.
    EffectChainMixMode type = static_cast<EffectChainMixMode>(int(v));
    (void)v; // this avoids a false warning with g++ 4.8.1

    // NOTE(Kshitij) : Removed the reference to m_pEffectChain
    // if (m_pEffectChain && type < EffectChainMixMode::NumMixModes) {
    //     m_pEffectChain->setMixMode(type);
    // }
    if (type < EffectChainMixMode::NumMixModes) {
        setMixMode(type);
    }
}

void EffectChainSlot::slotControlChainSelector(double v) {
    qDebug() << debugString() << "slotControlChainSelector" << v;
    if (v > 0) {
        emit(nextChain(m_iChainSlotNumber, m_pEffectChain));
    } else if (v < 0) {
        emit(prevChain(m_iChainSlotNumber, m_pEffectChain));
    }
}

void EffectChainSlot::slotControlChainNextPreset(double v) {
    qDebug() << debugString() << "slotControlChainNextPreset" << v;
    if (v > 0) {
        slotControlChainSelector(1);
    }
}

void EffectChainSlot::slotControlChainPrevPreset(double v) {
    //qDebug() << debugString() << "slotControlChainPrevPreset" << v;
    if (v > 0) {
        slotControlChainSelector(-1);
    }
}

void EffectChainSlot::slotChannelStatusChanged(const QString& group) {
    // NOTE(Kshitij) : Removed the reference to m_pEffectChain
    // if (m_pEffectChain) {
    //     ChannelInfo* pChannelInfo = m_channelInfoByName.value(group, NULL);
    //     if (pChannelInfo != NULL && pChannelInfo->pEnabled != NULL) {
    //         bool bEnable = pChannelInfo->pEnabled->toBool();
    //         if (bEnable) {
    //             m_pEffectChain->enableForInputChannel(pChannelInfo->handle_group);
    //         } else {
    //             m_pEffectChain->disableForInputChannel(pChannelInfo->handle_group);
    //         }
    //     }
    // }
    // TODO(Kshitij) : Check the avaibility of effect chain in this slot. Similarly in other functions
    // if (m_pEffectChain) {
        qDebug() << "slotChannelStatusChanged";
        ChannelInfo* pChannelInfo = m_channelInfoByName.value(group, NULL);
        if (pChannelInfo != NULL && pChannelInfo->pEnabled != NULL) {
            bool bEnable = pChannelInfo->pEnabled->toBool();
            if (bEnable) {
                enableForInputChannel(pChannelInfo->handle_group);
            } else {
                disableForInputChannel(pChannelInfo->handle_group);
            }
        }
    // }
}

unsigned int EffectChainSlot::getChainSlotNumber() const {
    return m_iChainSlotNumber;
}

QDomElement EffectChainSlot::toXml(QDomDocument* doc) const {
    QDomElement chainElement = doc->createElement(EffectXml::Chain);
    // NOTE(Kshitij) : Used isEmpty()
    // TODO(Kshitij) : Broken toXml function
    // IMPORTANT(Kshitij) : Broken toXml function
    // if (m_pEffectChain == nullptr) {
    // if (isEmpty()) {
        // ejected chains are stored empty <EffectChain/>
        return chainElement;
    // }

    XmlParse::addElement(*doc, chainElement, EffectXml::ChainId,
            m_pEffectChain->id());
    XmlParse::addElement(*doc, chainElement, EffectXml::ChainName,
            m_pEffectChain->name());
    XmlParse::addElement(*doc, chainElement, EffectXml::ChainDescription,
            m_pEffectChain->description());
    XmlParse::addElement(*doc, chainElement, EffectXml::ChainMixMode,
            EffectChain::mixModeToString(
                    static_cast<EffectChainMixMode>(
                            static_cast<int>(m_pControlChainMixMode->get()))));
    XmlParse::addElement(*doc, chainElement, EffectXml::ChainSuperParameter,
            QString::number(m_pControlChainSuperParameter->get()));

    QDomElement effectsElement = doc->createElement(EffectXml::EffectsRoot);
    for (const auto& pEffectSlot : m_slots) {
        QDomElement effectNode;
        if (pEffectSlot->getEffect()) {
            effectNode = pEffectSlot->toXml(doc);
        } else {
            // Create empty element to ensure effects stay in order
            // if there are empty slots before loaded slots.
            effectNode = doc->createElement(EffectXml::Effect);
        }
        effectsElement.appendChild(effectNode);
    }
    chainElement.appendChild(effectsElement);

    return chainElement;
}

void EffectChainSlot::loadChainSlotFromXml(const QDomElement& effectChainElement) {
    if (!effectChainElement.hasChildNodes()) {
        return;
    }

    // FIXME: mix mode is set in EffectChain::createFromXml

    m_pControlChainSuperParameter->set(XmlParse::selectNodeDouble(
                                          effectChainElement,
                                          EffectXml::ChainSuperParameter));

    QDomElement effectsElement = XmlParse::selectElement(effectChainElement,
                                                         EffectXml::EffectsRoot);
    QDomNodeList effectsNodeList = effectsElement.childNodes();
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i] != nullptr) {
            QDomNode effectNode = effectsNodeList.at(i);
            if (effectNode.isElement()) {
                QDomElement effectElement = effectNode.toElement();
                m_slots[i]->loadEffectSlotFromXml(effectElement);
            }
        }
    }
}
