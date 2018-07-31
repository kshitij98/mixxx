#include "effects/effectsmanager.h"

#include <QMetaType>
#include <QtAlgorithms>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include "engine/effects/engineeffectsmanager.h"
#include "effects/effectsbackend.h"
#include "effects/effectslot.h"
#include "effects/effectxmlelements.h"
#include "engine/effects/engineeffect.h"
#include "engine/effects/engineeffectchain.h"
#include "util/assert.h"

namespace {
const QString kStandardEffectRackGroup = "[EffectRack1]";
const QString kOutputEffectRackGroup = "[OutputEffectRack]";
const QString kQuickEffectRackGroup = "[QuickEffectRack1]";
const QString kEqualizerEffectRackGroup = "[EqualizerRack1]";
const QString kEffectGroupSeparator = "_";
const QString kGroupClose = "]";
const unsigned int kEffectMessagPipeFifoSize = 2048;
} // anonymous namespace


EffectsManager::EffectsManager(QObject* pParent, UserSettingsPointer pConfig,
                               ChannelHandleFactory* pChannelHandleFactory)
        : QObject(pParent),
          m_pChannelHandleFactory(pChannelHandleFactory),
          m_pConfig(pConfig),
          m_nextRequestId(0),
          m_loEqFreq(ConfigKey("[Mixer Profile]", "LoEQFrequency"), 0., 22040),
          m_hiEqFreq(ConfigKey("[Mixer Profile]", "HiEQFrequency"), 0., 22040),
          m_underDestruction(false) {
    qRegisterMetaType<EffectChainMixMode>("EffectChainMixMode");
    QPair<EffectsRequestPipe*, EffectsResponsePipe*> requestPipes =
            TwoWayMessagePipe<EffectsRequest*, EffectsResponse>::makeTwoWayMessagePipe(
                kEffectMessagPipeFifoSize, kEffectMessagPipeFifoSize, false, false);

    m_pRequestPipe.reset(requestPipes.first);
    m_pEngineEffectsManager = new EngineEffectsManager(requestPipes.second);

    m_pNumEffectsAvailable = new ControlObject(ConfigKey("[Master]", "num_effectsavailable"));
    m_pNumEffectsAvailable->setReadOnly();
}

EffectsManager::~EffectsManager() {
    m_underDestruction = true;

    // NOTE(Kshitij) : Use new functions for saving XML files
    // saveEffectChains();

    // The EffectChainSlots must be deleted before the EffectsBackends in case
    // there is an LV2 effect currently loaded.
    // ~LV2GroupState calls lilv_instance_free, which will segfault if called
    // after ~LV2Backend calls lilv_world_free.
    m_equalizerEffectChainSlots.clear();
    m_quickEffectChainSlots.clear();
    m_standardEffectChainSlots.clear();
    m_outputEffectChainSlot.clear();
    m_effectChainSlotsByGroup.clear();
    processEffectsResponses();

    m_effectsBackends.clear();
    for (QHash<qint64, EffectsRequest*>::iterator it = m_activeRequests.begin();
         it != m_activeRequests.end();) {
        delete it.value();
        it = m_activeRequests.erase(it);
    }

    // delete m_pHiEqFreq;
    // delete m_pLoEqFreq;
    delete m_pNumEffectsAvailable;
    // Safe because the Engine is deleted before EffectsManager. Also, it holds
    // a bare pointer to m_pRequestPipe so it is critical that it does not
    // outlast us.
    delete m_pEngineEffectsManager;
}

bool alphabetizeEffectManifests(EffectManifestPointer pManifest1,
                                EffectManifestPointer pManifest2) {
    // Sort built-in effects first before external plugins
    int backendNameComparision = static_cast<int>(pManifest1->backendType()) - static_cast<int>(pManifest2->backendType());
    int displayNameComparision = QString::localeAwareCompare(pManifest1->displayName(), pManifest2->displayName());
    return (backendNameComparision ? (backendNameComparision < 0) : (displayNameComparision < 0));
}

void EffectsManager::addEffectsBackend(EffectsBackendPointer pBackend) {
    VERIFY_OR_DEBUG_ASSERT(pBackend) {
        return;
    }
    m_effectsBackends.insert(pBackend->getType(), pBackend);

    QList<QString> backendEffects = pBackend->getEffectIds();
    for (const QString& effectId : backendEffects) {
        m_availableEffectManifests.append(pBackend->getManifest(effectId));
    }

    m_pNumEffectsAvailable->forceSet(m_availableEffectManifests.size());

    qSort(m_availableEffectManifests.begin(), m_availableEffectManifests.end(),
          alphabetizeEffectManifests);
}

bool EffectsManager::isAdoptMetaknobValueEnabled() const {
    return m_pConfig->getValue(ConfigKey("[Effects]", "AdoptMetaknobValue"), true);
}

void EffectsManager::registerInputChannel(const ChannelHandleAndGroup& handle_group) {
    VERIFY_OR_DEBUG_ASSERT(!m_registeredInputChannels.contains(handle_group)) {
        return;
    }
    m_registeredInputChannels.insert(handle_group);

    foreach (EffectChainSlotPointer pChainSlot, m_standardEffectChainSlots) {
        pChainSlot->registerInputChannel(handle_group);
    }
}

void EffectsManager::registerOutputChannel(const ChannelHandleAndGroup& handle_group) {
    VERIFY_OR_DEBUG_ASSERT(!m_registeredOutputChannels.contains(handle_group)) {
        return;
    }
    m_registeredOutputChannels.insert(handle_group);
}

void EffectsManager::loadStandardEffect(const int iChainSlotNumber,
        const int iEffectSlotNumber, const EffectManifestPointer pManifest) {
    auto pChainSlot = getStandardEffectChainSlot(iChainSlotNumber);
    if (pChainSlot) {
        loadEffect(pChainSlot, iEffectSlotNumber, pManifest);
    }
}

void EffectsManager::loadOutputEffect(const int iEffectSlotNumber,
    const EffectManifestPointer pManifest) {
    if (m_outputEffectChainSlot) {
        loadEffect(m_outputEffectChainSlot, iEffectSlotNumber, pManifest);
    }
}

void EffectsManager::loadQuickEffect(const QString& deckGroup,
        const int iEffectSlotNumber, const EffectManifestPointer pManifest) {
    auto pChainSlot = m_quickEffectChainSlots.value(deckGroup);
    VERIFY_OR_DEBUG_ASSERT(pChainSlot) {
        return;
    }
    loadEffect(pChainSlot, iEffectSlotNumber, pManifest);
}

void EffectsManager::loadEqualizerEffect(const QString& deckGroup,
        const int iEffectSlotNumber, const EffectManifestPointer pManifest) {
    auto pChainSlot = m_equalizerEffectChainSlots.value(deckGroup);
    VERIFY_OR_DEBUG_ASSERT(pChainSlot) {
        return;
    }
    loadEffect(pChainSlot, iEffectSlotNumber, pManifest);
}

void EffectsManager::loadEffect(EffectChainSlotPointer pChainSlot,
        const int iEffectSlotNumber, const EffectManifestPointer pManifest) {
    if (kEffectDebugOutput) {
        qDebug() << debugString() << "loading effect" << iEffectSlotNumber << pManifest;
    }
    pChainSlot->loadEffect(iEffectSlotNumber, pManifest,
                           createProcessor(pManifest));
}

std::unique_ptr<EffectProcessor> EffectsManager::createProcessor(
        const EffectManifestPointer pManifest) {
    if (!pManifest) {
        // This can be a valid request to unload an effect, so do not DEBUG_ASSERT.
        return std::unique_ptr<EffectProcessor>(nullptr);
    }
    EffectsBackendPointer pBackend = m_effectsBackends.value(pManifest->backendType());
    VERIFY_OR_DEBUG_ASSERT(pBackend) {
        return std::unique_ptr<EffectProcessor>(nullptr);
    }
    return pBackend->createProcessor(pManifest);
}

const QList<EffectManifestPointer> EffectsManager::getAvailableEffectManifestsFiltered(
        EffectManifestFilterFnc filter) const {
    if (filter == nullptr) {
        return m_availableEffectManifests;
    }

    QList<EffectManifestPointer> list;
    for (const auto& pManifest : m_availableEffectManifests) {
        if (filter(pManifest.data())) {
            list.append(pManifest);
        }
    }
    return list;
}

QString EffectsManager::getNextEffectId(const QString& effectId) {
    if (m_availableEffectManifests.isEmpty()) {
        return QString();
    }
    if (effectId.isNull()) {
        return m_availableEffectManifests.first()->id();
    }

    int index;
    for (index = 0; index < m_availableEffectManifests.size(); ++index) {
        if (effectId == m_availableEffectManifests.at(index)->id()) {
            break;
        }
    }
    if (++index >= m_availableEffectManifests.size()) {
        index = 0;
    }
    return m_availableEffectManifests.at(index)->id();
}

QString EffectsManager::getPrevEffectId(const QString& effectId) {
    if (m_availableEffectManifests.isEmpty()) {
        return QString();
    }
    if (effectId.isNull()) {
        return m_availableEffectManifests.last()->id();
    }

    int index;
    for (index = 0; index < m_availableEffectManifests.size(); ++index) {
        if (effectId == m_availableEffectManifests.at(index)->id()) {
            break;
        }
    }
    if (--index < 0) {
        index = m_availableEffectManifests.size() - 1;
    }
    return m_availableEffectManifests.at(index)->id();
}

void EffectsManager::getEffectManifestAndBackend(
        const QString& effectId,
        EffectManifestPointer* ppManifest, EffectsBackend** ppBackend) const {
    for (const auto& pBackend : m_effectsBackends) {
        if (pBackend->canInstantiateEffect(effectId)) {
            *ppManifest = pBackend->getManifest(effectId);
            *ppBackend = pBackend.data();
        }
    }
}

EffectManifestPointer EffectsManager::getManifestFromUniqueId(const QString& uid) const {
    if (kEffectDebugOutput) {
        qDebug() << "EffectsManager::getManifestFromUniqueId" << uid;
    }
    if (uid.isEmpty()) {
        // Do not DEBUG_ASSERT, this may be a valid request for a nullptr to
        // unload an effect.
        return EffectManifestPointer();
    }
    int delimiterIndex = uid.lastIndexOf(" ");
    EffectBackendType backendType = EffectManifest::backendTypeFromString(
            uid.mid(delimiterIndex+1));
    VERIFY_OR_DEBUG_ASSERT(backendType != EffectBackendType::Unknown) {
        // Mixxx 2.0 - 2.2 did not store the backend type in mixxx.cfg,
        // so this code will be executed once when upgrading to Mixxx 2.3.
        // This debug assertion is safe to ignore in that case. If it is
        // triggered at any later time, there is a bug somewhere.
        // Do not manipulate the string passed to this function, just pass
        // it directly to BuiltInBackend.
        return m_effectsBackends.value(EffectBackendType::BuiltIn)->getManifest(uid);
    }
    return m_effectsBackends.value(backendType)->getManifest(
            uid.mid(-1, delimiterIndex+1));
}

void EffectsManager::addStandardEffectChainSlots() {
    for (int i = 0; i < EffectsManager::kNumStandardEffectChains; ++i) {
        VERIFY_OR_DEBUG_ASSERT(!m_effectChainSlotsByGroup.contains(
                StandardEffectChainSlot::formatEffectChainSlotGroup(i))) {
            continue;
        }

        auto pChainSlot = StandardEffectChainSlotPointer(
            new StandardEffectChainSlot(i, this));

        m_standardEffectChainSlots.append(pChainSlot);
        m_effectChainSlotsByGroup.insert(pChainSlot->group(), pChainSlot);
    }
}

void EffectsManager::addOutputEffectChainSlot() {
    m_outputEffectChainSlot = OutputEffectChainSlotPointer(new OutputEffectChainSlot(this));
    m_effectChainSlotsByGroup.insert(m_outputEffectChainSlot->group(), m_outputEffectChainSlot);
}

EffectChainSlotPointer EffectsManager::getOutputEffectChainSlot() const {
    return m_outputEffectChainSlot;
}

EffectChainSlotPointer EffectsManager::getStandardEffectChainSlot(int unitNumber) const {
    VERIFY_OR_DEBUG_ASSERT(0 <= unitNumber || unitNumber < m_standardEffectChainSlots.size()) {
        return EffectChainSlotPointer();
    }
    return m_standardEffectChainSlots.at(unitNumber);
}

void EffectsManager::addEqualizerEffectChainSlot(const QString& deckGroupName) {
    VERIFY_OR_DEBUG_ASSERT(!m_equalizerEffectChainSlots.contains(
            EqualizerEffectChainSlot::formatEffectChainSlotGroup(deckGroupName))) {
        return;
    }

    auto pChainSlot = EqualizerEffectChainSlotPointer(
            new EqualizerEffectChainSlot(deckGroupName, this));
    m_equalizerEffectChainSlots.insert(deckGroupName, pChainSlot);

    m_effectChainSlotsByGroup.insert(pChainSlot->group(), pChainSlot);
}

void EffectsManager::addQuickEffectChainSlot(const QString& deckGroupName) {
    VERIFY_OR_DEBUG_ASSERT(!m_quickEffectChainSlots.contains(
            QuickEffectChainSlot::formatEffectChainSlotGroup(deckGroupName))) {
        return;
    }

    auto pChainSlot = QuickEffectChainSlotPointer(
        new QuickEffectChainSlot(deckGroupName, this));

    m_quickEffectChainSlots.insert(deckGroupName, pChainSlot);
    m_effectChainSlotsByGroup.insert(pChainSlot->group(), pChainSlot);
}

EffectChainSlotPointer EffectsManager::getEffectChainSlot(const QString& group) const {
    return m_effectChainSlotsByGroup.value(group);
}

EffectSlotPointer EffectsManager::getEffectSlot(
        const QString& group) {
    QRegExp intRegEx(".*(\\d+).*");

    QStringList parts = group.split(kEffectGroupSeparator);

    // NOTE(Kshitij) : Assuming the group is valid
    const QString chainGroup = parts.at(0) + kEffectGroupSeparator + parts.at(1) + kGroupClose;
    EffectChainSlotPointer pChainSlot = getEffectChainSlot(chainGroup);
    VERIFY_OR_DEBUG_ASSERT(pChainSlot) {
        return EffectSlotPointer();
    }

    intRegEx.indexIn(parts.at(2));
    EffectSlotPointer pEffectSlot =
            pChainSlot->getEffectSlot(intRegEx.cap(1).toInt() - 1);
    return pEffectSlot;
}

EffectKnobParameterSlotPointer EffectsManager::getEffectKnobParameterSlot(
        const ConfigKey& configKey) {
    EffectSlotPointer pEffectSlot =
             getEffectSlot(configKey.group);
    VERIFY_OR_DEBUG_ASSERT(pEffectSlot) {
        return EffectKnobParameterSlotPointer();
    }

    QRegExp intRegEx(".*(\\d+).*");
    intRegEx.indexIn(configKey.item);
    EffectKnobParameterSlotPointer pParameterSlot =
            pEffectSlot->getEffectKnobParameterSlot(intRegEx.cap(1).toInt() - 1);
    return pParameterSlot;
}

EffectButtonParameterSlotPointer EffectsManager::getEffectButtonParameterSlot(
        const ConfigKey& configKey) {
    EffectSlotPointer pEffectSlot =
             getEffectSlot(configKey.group);
    VERIFY_OR_DEBUG_ASSERT(pEffectSlot) {
        return EffectButtonParameterSlotPointer();
    }

    QRegExp intRegEx(".*(\\d+).*");
    intRegEx.indexIn(configKey.item);
    EffectButtonParameterSlotPointer pParameterSlot =
            pEffectSlot->getEffectButtonParameterSlot(intRegEx.cap(1).toInt() - 1);
    return pParameterSlot;
}

void EffectsManager::setEffectVisibility(EffectManifestPointer pManifest, bool visible) {
    if (visible && !m_visibleEffectManifests.contains(pManifest)) {
        auto insertion_point = std::lower_bound(m_visibleEffectManifests.begin(),
                                                m_visibleEffectManifests.end(),
                                                pManifest, alphabetizeEffectManifests);
        m_visibleEffectManifests.insert(insertion_point, pManifest);
        emit(visibleEffectsUpdated());
    } else if (!visible) {
        m_visibleEffectManifests.removeOne(pManifest);
        emit(visibleEffectsUpdated());
    }
}

bool EffectsManager::getEffectVisibility(EffectManifestPointer pManifest) {
    return m_visibleEffectManifests.contains(pManifest);
}

void EffectsManager::setup() {
    // Add postfader effect chain slots
    addStandardEffectChainSlots();
    addOutputEffectChainSlot();
}

// Load Effect Chains from effects.json
void EffectsManager::loadEffectChains() {
    QDir settingsPath(m_pConfig->getSettingsPath());
    QFile file(settingsPath.absoluteFilePath("effects.json"));
    file.open(QFile::ReadOnly);
    QJsonDocument doc = QJsonDocument().fromJson(file.readAll());
    QJsonObject object = doc.object();

    qDebug() << object;
}

void EffectsManager::refreshAllChainSlots() {
    for (auto& pChainSlot : m_standardEffectChainSlots) {
        pChainSlot->reloadAllEffects();
    }
    for (auto& pChainSlot : m_equalizerEffectChainSlots) {
        pChainSlot->reloadAllEffects();
    }
    for (auto& pChainSlot : m_quickEffectChainSlots) {
        pChainSlot->reloadAllEffects();
    }
}

bool EffectsManager::writeRequest(EffectsRequest* request) {
    if (m_underDestruction) {
        // Catch all delete Messages since the engine is already down
        // and we cannot wait for a communication cycle
        collectGarbage(request);
    }

    if (m_pRequestPipe.isNull()) {
        delete request;
        return false;
    }

    // This is effectively only garbage collection at this point so only deal
    // with responses when writing new requests.
    processEffectsResponses();

    request->request_id = m_nextRequestId++;
    // TODO(XXX) use preallocated requests to avoid delete calls from engine
    if (m_pRequestPipe->writeMessages(&request, 1) == 1) {
        m_activeRequests[request->request_id] = request;
        return true;
    }
    delete request;
    return false;
}

void EffectsManager::processEffectsResponses() {
    if (m_pRequestPipe.isNull()) {
        return;
    }

    EffectsResponse response;
    while (m_pRequestPipe->readMessages(&response, 1) == 1) {
        QHash<qint64, EffectsRequest*>::iterator it =
                m_activeRequests.find(response.request_id);

        VERIFY_OR_DEBUG_ASSERT(it != m_activeRequests.end()) {
            qWarning() << debugString()
                       << "WARNING: EffectsResponse with an inactive request_id:"
                       << response.request_id;
        }

        while (it != m_activeRequests.end() &&
               it.key() == response.request_id) {
            EffectsRequest* pRequest = it.value();

            // Don't check whether the response was successful here because
            // specific errors should be caught with DEBUG_ASSERTs in
            // EngineEffectsManager and functions it calls to handle requests.

            collectGarbage(pRequest);

            delete pRequest;
            it = m_activeRequests.erase(it);
        }
    }
}

void EffectsManager::collectGarbage(const EffectsRequest* pRequest) {
    if (pRequest->type == EffectsRequest::REMOVE_EFFECT_FROM_CHAIN) {
        if (kEffectDebugOutput) {
            qDebug() << debugString() << "delete" << pRequest->RemoveEffectFromChain.pEffect;
        }
        delete pRequest->RemoveEffectFromChain.pEffect;
    } else if (pRequest->type == EffectsRequest::REMOVE_EFFECT_CHAIN) {
        if (kEffectDebugOutput) {
            qDebug() << debugString() << "delete" << pRequest->RemoveEffectChain.pChain;
        }
        delete pRequest->RemoveEffectChain.pChain;
    } else if (pRequest->type == EffectsRequest::DISABLE_EFFECT_CHAIN_FOR_INPUT_CHANNEL) {
        if (kEffectDebugOutput) {
            qDebug() << debugString() << "deleting states for input channel" << pRequest->DisableInputChannelForChain.pChannelHandle << "for EngineEffectChain" << pRequest->pTargetChain;
        }
        pRequest->pTargetChain->deleteStatesForInputChannel(
                pRequest->DisableInputChannelForChain.pChannelHandle);
    }
}
