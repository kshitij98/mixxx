#ifndef EFFECTSLOT_H
#define EFFECTSLOT_H

#include <QObject>
#include <QSharedPointer>
#include <QString>

#include "control/controlencoder.h"
#include "control/controlobject.h"
#include "control/controlpotmeter.h"
#include "control/controlpushbutton.h"
#include "controllers/softtakeover.h"
#include "engine/channelhandle.h"
#include "engine/engine.h"
#include "engine/effects/engineeffect.h"
#include "effects/effectbuttonparameterslot.h"
#include "effects/effectmanifest.h"
#include "effects/effectknobparameterslot.h"
#include "util/class.h"

class EffectProcessor;
class EffectSlot;
class EffectState;
class EffectsManager;
class EngineEffect;
class EngineEffectChain;
class ControlProxy;
class EffectKnobParameterSlot;

class EffectSlot : public QObject {
    Q_OBJECT
  public:
    typedef bool (*ParameterFilterFnc)(EffectManifestParameterPointer);

    EffectSlot(const QString& group,
               EffectsManager* pEffectsManager,
               const unsigned int iEffectNumber,
               EngineEffectChain* pEngineEffectChain);
    virtual ~EffectSlot();

    void addToEngine(std::unique_ptr<EffectProcessor>,
            const QSet<ChannelHandleAndGroup>& activeInputChannels);
    void removeFromEngine();

    // Call with nullptr for pManifest and pProcessor to unload an effect
    void loadEffect(const EffectManifestPointer pManifest,
            std::unique_ptr<EffectProcessor> pProcessor,
            const QSet<ChannelHandleAndGroup>& activeChannels);

    inline int getEffectSlotNumber() const {
        return m_iEffectNumber;
    }

    inline bool isLoaded() const {
        return m_pEngineEffect != nullptr;
    }

    unsigned int numParameterSlots() const;
    EffectKnobParameterSlotPointer addEffectKnobParameterSlot();
    EffectKnobParameterSlotPointer getEffectKnobParameterSlot(unsigned int slotNumber);
    EffectKnobParameterSlotPointer getEffectKnobParameterSlotForConfigKey(unsigned int slotNumber);

    unsigned int numButtonParameterSlots() const;
    EffectButtonParameterSlotPointer addEffectButtonParameterSlot();
    EffectButtonParameterSlotPointer getEffectButtonParameterSlot(unsigned int slotNumber);

    double getMetaParameter() const;

    // Ensures that Softtakover is bypassed for the following
    // ChainParameterChange. Uses for testing only
    void syncSofttakeover();

    const QString& getGroup() const {
        return m_group;
    }

    QDomElement toXml(QDomDocument* doc) const;
    void loadEffectSlotFromXml(const QDomElement& effectElement);

    EffectState* createState(const mixxx::EngineParameters& bufferParameters);

    EffectManifestPointer getManifest() const;

    static bool isButtonParameter(EffectManifestParameterPointer parameter);
    static bool isKnobParameter(EffectManifestParameterPointer parameter);

    EffectManifestParameterPointer getFilteredParameterForSlot(
            ParameterFilterFnc filterFnc, unsigned int slotNumber);
    EffectManifestParameterPointer getKnobParameterForSlot(unsigned int slotNumber);
    EffectManifestParameterPointer getButtonParameterForSlot(unsigned int slotNumber);

    void setEnabled(bool enabled);

    EngineEffect* getEngineEffect();



    // static EffectPointer createFromXml(EffectsManager* pEffectsManager,
    //                              const QDomElement& element);

    double getMetaknobDefault();
    void reload(const QSet<ChannelHandleAndGroup>& activeInputChannels);

  public slots:
    void setMetaParameter(double v, bool force = false);

    void slotNextEffect(double v);
    void slotPrevEffect(double v);
    void slotClear(double v);
    void slotEffectSelector(double v);
    void slotEffectMetaParameter(double v, bool force = false);

  signals:
    void effectChanged();

  private slots:
    void updateEngineState();

  private:
    QString debugString() const {
        return QString("EffectSlot(%1)").arg(m_group);
    }

    void sendParameterUpdate();
    void unloadEffect();

    const unsigned int m_iEffectNumber;
    const QString m_group;
    UserSettingsPointer m_pConfig;
    EffectsManager* m_pEffectsManager;
    EffectManifestPointer m_pManifest;
    EngineEffect* m_pEngineEffect;
    EngineEffectChain* m_pEngineEffectChain;
    QList<EffectKnobParameterSlotPointer> m_knobParameterSlots;
    QList<EffectButtonParameterSlotPointer> m_buttonParameterSlots;

    ControlObject* m_pControlLoaded;
    ControlPushButton* m_pControlEnabled;
    ControlObject* m_pControlNumParameters;
    ControlObject* m_pControlNumParameterSlots;
    ControlObject* m_pControlNumButtonParameters;
    ControlObject* m_pControlNumButtonParameterSlots;
    ControlObject* m_pControlNextEffect;
    ControlObject* m_pControlPrevEffect;
    ControlEncoder* m_pControlEffectSelector;
    ControlObject* m_pControlClear;
    ControlPotmeter* m_pControlMetaParameter;

    SoftTakeover* m_pMetaknobSoftTakeover;

    DISALLOW_COPY_AND_ASSIGN(EffectSlot);
};

#endif /* EFFECTSLOT_H */
