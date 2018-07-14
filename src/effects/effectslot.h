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
#include "effects/effectparameter.h"
#include "effects/effectparameterslot.h"
#include "util/class.h"

class EffectProcessor;
class EffectSlot;
class EffectState;
class EffectsManager;
class EngineEffect;
class EngineEffectChain;
class ControlProxy;
class EffectParameter;
class EffectParameterSlot;

class EffectSlot : public QObject {
    Q_OBJECT
  public:
    typedef bool (*ParameterFilterFnc)(EffectParameter*);

    EffectSlot(const QString& group,
               EffectsManager* pEffectsManager,
               const unsigned int iEffectNumber,
               EngineEffectChain* pEngineEffectChain);
    virtual ~EffectSlot();

    // Call with nullptr for pManifest and pProcessor to unload an effect
    void loadEffect(EffectManifestPointer pManifest,
            std::unique_ptr<EffectProcessor> pProcessor,
            const QSet<ChannelHandleAndGroup>& activeChannels);

    inline int getEffectSlotNumber() const {
        return m_iEffectNumber;
    }

    inline const bool isLoaded() const {
        return m_pEngineEffect != nullptr;
    }

    unsigned int numParameterSlots() const;
    EffectParameterSlotPointer addEffectParameterSlot();
    EffectParameterSlotPointer getEffectParameterSlot(unsigned int slotNumber);
    EffectParameterSlotPointer getEffectParameterSlotForConfigKey(unsigned int slotNumber);
    inline const QList<EffectParameterSlotPointer>& getEffectParameterSlots() const {
        return m_parameterSlots;
    };

    unsigned int numButtonParameterSlots() const;
    EffectButtonParameterSlotPointer addEffectButtonParameterSlot();
    EffectButtonParameterSlotPointer getEffectButtonParameterSlot(unsigned int slotNumber);
    inline const QList<EffectButtonParameterSlotPointer>& getEffectButtonParameterSlots() const {
        return m_buttonParameters;
    };

    double getMetaParameter() const;

    // ensures that Softtakover is bypassed for the following
    // ChainParameterChange. Uses for testing only
    void syncSofttakeover();

    const QString& getGroup() const {
        return m_group;
    }

    QDomElement toXml(QDomDocument* doc) const;
    void loadEffectSlotFromXml(const QDomElement& effectElement);

    EffectState* createState(const mixxx::EngineParameters& bufferParameters);

    EffectManifestPointer getManifest() const;

    unsigned int numKnobParameters() const;
    unsigned int numButtonParameters() const;

    static bool isButtonParameter(EffectParameter* parameter);
    static bool isKnobParameter(EffectParameter* parameter);

    EffectParameter* getFilteredParameterForSlot(
            ParameterFilterFnc filterFnc, unsigned int slotNumber);
    EffectParameter* getKnobParameterForSlot(unsigned int slotNumber);
    EffectParameter* getButtonParameterForSlot(unsigned int slotNumber);

    EffectParameter* getParameterById(const QString& id) const;
    EffectParameter* getButtonParameterById(const QString& id) const;

    void setEnabled(bool enabled);
    bool enabled() const;

    EngineEffect* getEngineEffect();

    // static EffectPointer createFromXml(EffectsManager* pEffectsManager,
    //                              const QDomElement& element);
    void addToEngine(std::unique_ptr<EffectProcessor>,
            const QSet<ChannelHandleAndGroup>& activeInputChannels);
    void removeFromEngine();

    double getMetaknobDefault();

  public slots:
    // Request that this EffectSlot load the given Effect
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
    QList<EffectParameter*> m_parameters;
    QMap<QString, EffectParameter*> m_parametersById;
    EngineEffectChain* m_pEngineEffectChain;
    QList<EffectParameterSlotPointer> m_parameterSlots;
    QList<EffectButtonParameterSlotPointer> m_buttonParameters;

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

    SoftTakeover* m_pSoftTakeover;

    DISALLOW_COPY_AND_ASSIGN(EffectSlot);
};

#endif /* EFFECTSLOT_H */
