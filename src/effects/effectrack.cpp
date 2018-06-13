#include "effects/effectrack.h"

#include "effects/effectsmanager.h"
#include "effects/effectchainmanager.h"
#include "effects/effectslot.h"
#include "engine/effects/engineeffectrack.h"

#include "util/assert.h"

EffectRack::EffectRack(EffectsManager* pEffectsManager,
                       EffectChainManager* pEffectChainManager,
                       const unsigned int iRackNumber,
                       const QString& group, SignalProcessingStage stage)
        : m_pEngineEffectRack(nullptr),
          m_pEffectsManager(pEffectsManager),
          m_pEffectChainManager(pEffectChainManager),
          m_signalProcessingStage(stage),
          m_iRackNumber(iRackNumber),
          m_group(group),
          m_controlNumEffectChainSlots(ConfigKey(m_group, "num_effectunits")),
          m_controlClearRack(ConfigKey(m_group, "clear")) {
    connect(&m_controlClearRack, SIGNAL(valueChanged(double)),
            this, SLOT(slotClearRack(double)));
    m_controlNumEffectChainSlots.setReadOnly();
    addToEngine();
}

EffectRack::~EffectRack() {
    m_effectChainSlots.clear();
    removeFromEngine();
    //qDebug() << "EffectRack::~EffectRack()";
}

EngineEffectRack* EffectRack::getEngineEffectRack() {
    return m_pEngineEffectRack;
}

void EffectRack::addToEngine() {
    m_pEngineEffectRack = new EngineEffectRack(m_iRackNumber);
    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::ADD_EFFECT_RACK;
    pRequest->AddEffectRack.pRack = m_pEngineEffectRack;
    pRequest->AddEffectRack.signalProcessingStage = m_signalProcessingStage;
    m_pEffectsManager->writeRequest(pRequest);

    for (int i = 0; i < m_effectChainSlots.size(); ++i) {
        EffectChainSlotPointer pSlot = m_effectChainSlots[i];
        pSlot->updateEngineState();
    }
}

void EffectRack::removeFromEngine() {
    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::REMOVE_EFFECT_RACK;
    pRequest->RemoveEffectRack.signalProcessingStage = m_signalProcessingStage;
    pRequest->RemoveEffectRack.pRack = m_pEngineEffectRack;
    m_pEffectsManager->writeRequest(pRequest);
    m_pEngineEffectRack = NULL;
}

void EffectRack::registerInputChannel(const ChannelHandleAndGroup& handle_group) {
    foreach (EffectChainSlotPointer pChainSlot, m_effectChainSlots) {
        pChainSlot->registerInputChannel(handle_group);
    }
}

void EffectRack::slotClearRack(double v) {
    if (v > 0) {
        foreach (EffectChainSlotPointer pChainSlot, m_effectChainSlots) {
            pChainSlot->clear();
        }
    }
}

int EffectRack::numEffectChainSlots() const {
    return m_effectChainSlots.size();
}

void EffectRack::addEffectChainSlotInternal(EffectChainSlotPointer pChainSlot) {
    m_effectChainSlots.append(pChainSlot);
    m_controlNumEffectChainSlots.forceSet(
        m_controlNumEffectChainSlots.get() + 1);

    // qDebug() << "Total effect chain slots = " << m_controlNumEffectChainSlots.get();
}

EffectChainSlotPointer EffectRack::getEffectChainSlot(int i) {
    if (i < 0 || i >= m_effectChainSlots.size()) {
        qWarning() << "WARNING: Invalid index for getEffectChainSlot";
        return EffectChainSlotPointer();
    }
    return m_effectChainSlots[i];
}

void EffectRack::maybeLoadEffect(const unsigned int iChainSlotNumber,
                                 const unsigned int iEffectSlotNumber,
                                 const QString& id) {
    if (iChainSlotNumber >= static_cast<unsigned int>(m_effectChainSlots.size())) {
        return;
    }

    EffectChainSlotPointer pChainSlot = m_effectChainSlots[iChainSlotNumber];
    if (pChainSlot == nullptr) {
        return;
    }
    EffectSlotPointer pEffectSlot = pChainSlot->getEffectSlot(iEffectSlotNumber);

    bool loadNew = false;
    if (pEffectSlot == nullptr || pEffectSlot->getEffect() == nullptr) {
        loadNew = true;
    } else if (id != pEffectSlot->getEffect()->getManifest()->id()) {
        loadNew = true;
    }

    if (loadNew) {
        EffectPointer pEffect = m_pEffectsManager->instantiateEffect(id);
        pChainSlot->replaceEffect(iEffectSlotNumber, pEffect);
    }
}

void EffectRack::loadNextEffect(const unsigned int iChainSlotNumber,
                                const unsigned int iEffectSlotNumber,
                                EffectPointer pEffect) {
    if (iChainSlotNumber >= static_cast<unsigned int>(m_effectChainSlots.size())) {
        return;
    }

    QString effectId = pEffect ? pEffect->getManifest()->id() : QString();
    QString nextEffectId = m_pEffectsManager->getNextEffectId(effectId);
    EffectPointer pNextEffect = m_pEffectsManager->instantiateEffect(nextEffectId);

    EffectChainSlotPointer pChainSlot = m_effectChainSlots[iChainSlotNumber];
    pChainSlot->replaceEffect(iEffectSlotNumber, pNextEffect);
}


void EffectRack::loadPrevEffect(const unsigned int iChainSlotNumber,
                                const unsigned int iEffectSlotNumber,
                                EffectPointer pEffect) {
    if (iChainSlotNumber >= static_cast<unsigned int>(m_effectChainSlots.size())) {
        return;
    }

    QString effectId = pEffect ? pEffect->getManifest()->id() : QString();
    QString prevEffectId = m_pEffectsManager->getPrevEffectId(effectId);
    EffectPointer pPrevEffect = m_pEffectsManager->instantiateEffect(prevEffectId);

    EffectChainSlotPointer pChainSlot = m_effectChainSlots[iChainSlotNumber];
    pChainSlot->replaceEffect(iEffectSlotNumber, pPrevEffect);
}

QDomElement EffectRack::toXml(QDomDocument* doc) const {
    QDomElement rackElement = doc->createElement("Rack");
    // QDomElement groupElement = doc->createElement("Group");
    // QDomText groupText = doc->createTextNode(m_group);
    // groupElement.appendChild(groupText);
    // rackElement.appendChild(groupElement);

    // QDomElement chainsElement = doc->createElement("Chains");
    // for (EffectChainSlotPointer pChainSlot : m_effectChainSlots) {
    //     QDomElement chain = pChainSlot->toXml(doc);
    //     chainsElement.appendChild(chain);
    // }
    // rackElement.appendChild(chainsElement);
    return rackElement;
}

void EffectRack::refresh() {
    for (const auto& pChainSlot: m_effectChainSlots) {
        pChainSlot->refreshAllEffects();
    }
}

bool EffectRack::isAdoptMetaknobValueEnabled() const {
    return m_pEffectChainManager->isAdoptMetaknobValueEnabled();
}

StandardEffectRack::StandardEffectRack(EffectsManager* pEffectsManager,
                                       EffectChainManager* pChainManager,
                                       const unsigned int iRackNumber)
        : EffectRack(pEffectsManager, pChainManager, iRackNumber,
                     formatGroupString(iRackNumber), SignalProcessingStage::Postfader) {
    for (int i = 0; i < EffectChainManager::kNumStandardEffectChains; ++i) {
        addEffectChainSlot();
    }
}

EffectChainSlotPointer StandardEffectRack::addEffectChainSlot() {
    int iChainSlotNumber = numEffectChainSlots();

    QString group = formatEffectChainSlotGroupString(getRackNumber(),
                                                     iChainSlotNumber);


    EffectChainSlot* pChainSlot =
            new EffectChainSlot(this, group, iChainSlotNumber, m_pEffectsManager);

    for (int i = 0; i < kNumEffectsPerUnit; ++i) {
        pChainSlot->addEffectSlot(
                StandardEffectRack::formatEffectSlotGroupString(
                        getRackNumber(), iChainSlotNumber, i));
    }

    connect(pChainSlot, SIGNAL(nextEffect(unsigned int, unsigned int, EffectPointer)),
            this, SLOT(loadNextEffect(unsigned int, unsigned int, EffectPointer)));
    connect(pChainSlot, SIGNAL(prevEffect(unsigned int, unsigned int, EffectPointer)),
            this, SLOT(loadPrevEffect(unsigned int, unsigned int, EffectPointer)));

    // Register all the existing channels with the new EffectChain.
    const QSet<ChannelHandleAndGroup>& registeredChannels =
            m_pEffectChainManager->registeredInputChannels();
    for (const ChannelHandleAndGroup& handle_group : registeredChannels) {
        pChainSlot->registerInputChannel(handle_group);
    }

    EffectChainSlotPointer pChainSlotPointer = EffectChainSlotPointer(pChainSlot);
    addEffectChainSlotInternal(pChainSlotPointer);

    return pChainSlotPointer;
}

OutputEffectRack::OutputEffectRack(EffectsManager* pEffectsManager,
                                   EffectChainManager* pChainManager)
        : EffectRack(pEffectsManager, pChainManager, 0,
                     "[OutputEffectRack]", SignalProcessingStage::Postfader) {

    const QString unitGroup = "[OutputEffectRack_[Master]]";
    // Hard code only one EffectChainSlot
    EffectChainSlot* pChainSlot = new EffectChainSlot(this, unitGroup, 0, m_pEffectsManager, unitGroup);
    pChainSlot->addEffectSlot("[OutputEffectRack_[Master]_Effect1]");

    connect(pChainSlot, SIGNAL(nextEffect(unsigned int, unsigned int, EffectPointer)),
            this, SLOT(loadNextEffect(unsigned int, unsigned int, EffectPointer)));
    connect(pChainSlot, SIGNAL(prevEffect(unsigned int, unsigned int, EffectPointer)),
            this, SLOT(loadPrevEffect(unsigned int, unsigned int, EffectPointer)));

    // Register the master channel.
    const ChannelHandleAndGroup* masterHandleAndGroup = nullptr;

    // TODO(Be): Remove this hideous hack to get the ChannelHandleAndGroup
    const QSet<ChannelHandleAndGroup>& registeredChannels =
            m_pEffectChainManager->registeredInputChannels();
    for (const ChannelHandleAndGroup& handle_group : registeredChannels) {
        if (handle_group.name() == "[MasterOutput]") {
            masterHandleAndGroup = &handle_group;
            break;
        }
    }
    DEBUG_ASSERT(masterHandleAndGroup != nullptr);

    pChainSlot->registerInputChannel(*masterHandleAndGroup);

    // TODO(Kshitij) : Make the following function private after EffectRack
    // layer is removed.
    pChainSlot->enableForInputChannel(*masterHandleAndGroup);
    pChainSlot->setMix(1.0);

    EffectChainSlotPointer pChainSlotPointer = EffectChainSlotPointer(pChainSlot);
    addEffectChainSlotInternal(pChainSlotPointer);
}

PerGroupRack::PerGroupRack(EffectsManager* pEffectsManager,
                           EffectChainManager* pChainManager,
                           const unsigned int iRackNumber,
                           const QString& group)
        : EffectRack(pEffectsManager, pChainManager, iRackNumber, group,
                     SignalProcessingStage::Prefader) {
}

void PerGroupRack::setupForGroup(const QString& groupName) {
    VERIFY_OR_DEBUG_ASSERT(!m_groupToChainSlot.contains(groupName)) {
        return;
    }

    int iChainSlotNumber = m_groupToChainSlot.size();
    QString chainSlotGroup = formatEffectChainSlotGroupForGroup(
        getRackNumber(), iChainSlotNumber, groupName);
    qDebug() << "Chain slot group = " << chainSlotGroup;
    EffectChainSlot* pChainSlot = new EffectChainSlot(this, chainSlotGroup,
                                                      iChainSlotNumber,
                                                      m_pEffectsManager,
                                                      chainSlotGroup);
    EffectChainSlotPointer pChainSlotPointer(pChainSlot);
    addEffectChainSlotInternal(pChainSlotPointer);
    m_groupToChainSlot[groupName] = pChainSlotPointer;

    // TODO(rryan): Set up next/prev signals.

    // Set the chain to be fully wet.
    pChainSlot->setMix(1.0);
    pChainSlot->updateEngineState();

    // TODO(rryan): remove.
    const ChannelHandleAndGroup* handleAndGroup = nullptr;
    for (const ChannelHandleAndGroup& handle_group :
             m_pEffectChainManager->registeredInputChannels()) {
        if (handle_group.name() == groupName) {
            handleAndGroup = &handle_group;
            break;
        }
    }
    DEBUG_ASSERT(handleAndGroup != nullptr);

    // Register this channel alone with the chain slot.
    pChainSlot->registerInputChannel(*handleAndGroup);

    // Add a single effect slot
    pChainSlot->addEffectSlot(formatEffectSlotGroupString(0, groupName));
    // DlgPrefEq loads the Effect with loadEffectToGroup

    configureEffectChainSlotForGroup(pChainSlotPointer, groupName);
}

bool PerGroupRack::loadEffectToGroup(const QString& groupName, EffectPointer pEffect) {
    EffectChainSlotPointer pChainSlot = getGroupEffectChainSlot(groupName);
    VERIFY_OR_DEBUG_ASSERT(pChainSlot) {
        return false;
    }

    pChainSlot->replaceEffect(0, pEffect);
    if (pEffect != nullptr) {
        pEffect->setEnabled(true);
    }
    return true;
}

EffectChainSlotPointer PerGroupRack::getGroupEffectChainSlot(const QString& group) {
    return m_groupToChainSlot[group];
}

QuickEffectRack::QuickEffectRack(EffectsManager* pEffectsManager,
                                 EffectChainManager* pChainManager,
                                 const unsigned int iRackNumber)
        : PerGroupRack(pEffectsManager, pChainManager, iRackNumber,
                       QuickEffectRack::formatGroupString(iRackNumber)) {
}

void QuickEffectRack::configureEffectChainSlotForGroup(
        EffectChainSlotPointer pSlot, const QString& groupName) {
    Q_UNUSED(groupName);
    // Set the parameter default value to 0.5 (neutral).
    pSlot->setSuperParameter(0.5);
    pSlot->setSuperParameterDefaultValue(0.5);
}

bool QuickEffectRack::loadEffectToGroup(const QString& groupName,
                                        EffectPointer pEffect) {
    PerGroupRack::loadEffectToGroup(groupName, pEffect);
    EffectChainSlotPointer pChainSlot = getGroupEffectChainSlot(groupName);
    // Force update metaknobs and parameters to match state of superknob
    pChainSlot->setSuperParameter(pChainSlot->getSuperParameter(), true);
    return true;
}

EqualizerRack::EqualizerRack(EffectsManager* pEffectsManager,
                             EffectChainManager* pChainManager,
                             const unsigned int iRackNumber)
        : PerGroupRack(pEffectsManager, pChainManager, iRackNumber,
                       EqualizerRack::formatGroupString(iRackNumber)) {
}

void EqualizerRack::configureEffectChainSlotForGroup(EffectChainSlotPointer pSlot,
                                                     const QString& groupName) {
    // Create aliases for legacy EQ controls.
    // NOTE(rryan): If we ever add a second EqualizerRack then we need to make
    // these only apply to the first.
    EffectSlotPointer pEffectSlot = pSlot->getEffectSlot(0);
    if (pEffectSlot) {
        const QString& effectSlotGroup = pEffectSlot->getGroup();
        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterLow"),
                                          ConfigKey(effectSlotGroup, "parameter1"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterMid"),
                                          ConfigKey(effectSlotGroup, "parameter2"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterHigh"),
                                          ConfigKey(effectSlotGroup, "parameter3"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterLowKill"),
                                          ConfigKey(effectSlotGroup, "button_parameter1"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterMidKill"),
                                          ConfigKey(effectSlotGroup, "button_parameter2"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterHighKill"),
                                          ConfigKey(effectSlotGroup, "button_parameter3"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterLow_loaded"),
                                          ConfigKey(effectSlotGroup, "parameter1_loaded"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterMid_loaded"),
                                          ConfigKey(effectSlotGroup, "parameter2_loaded"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterHigh_loaded"),
                                          ConfigKey(effectSlotGroup, "parameter3_loaded"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterLowKill_loaded"),
                                          ConfigKey(effectSlotGroup, "button_parameter1_loaded"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterMidKill_loaded"),
                                          ConfigKey(effectSlotGroup, "button_parameter2_loaded"));

        ControlDoublePrivate::insertAlias(ConfigKey(groupName, "filterHighKill_loaded"),
                                          ConfigKey(effectSlotGroup, "button_parameter3_loaded"));
    }
}
