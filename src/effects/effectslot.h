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
#include "effects/effectbuttonparameterslot.h"
#include "effects/effectinstantiator.h"
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


class EffectSlot : public QObject {
    Q_OBJECT
  public:
    typedef bool (*ParameterFilterFnc)(EffectParameter*);

    EffectSlot(const QString& group,
               const unsigned int iEffectNumber);
    virtual ~EffectSlot();

    inline int getEffectSlotNumber() const {
        return m_iEffectNumber;
    }

    bool isLoaded() const {
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

    // Unload the currently loaded effect
    void clear();

    const QString& getGroup() const {
        return m_group;
    }

    QDomElement toXml(QDomDocument* doc) const;
    void loadEffectSlotFromXml(const QDomElement& effectElement);


    // NOTE(Kshitij) : START EFFECT
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

    void addToEngine(EffectsManager* pEffectsManager,
            EffectInstantiatorPointer pInstantiator,
            const QSet<ChannelHandleAndGroup>& activeInputChannels);
    void removeFromEngine();
    void updateEngineState();

    // static EffectPointer createFromXml(EffectsManager* pEffectsManager,
    //                              const QDomElement& element);

    double getMetaknobDefault();
    // NOTE(Kshitij) : END EFFECT


  public slots:
    // Request that this EffectSlot load the given Effect
    void setMetaParameter(double v, bool force = false);

    void loadEffectToSlot(EffectsManager* pEffectsManager = nullptr,
            EffectManifestPointer pManifest = EffectManifestPointer(),
            EffectInstantiatorPointer pInstantiator = EffectInstantiatorPointer(),
            const QSet<ChannelHandleAndGroup>& activeChannels = QSet<ChannelHandleAndGroup>());
    // kshitij : check if bool adoptMetaknobPosition is needed here
    // kshitij : add a default value for adoptMeta...

    // NOTE(Kshitij) : remove
    // void slotEnabled(double v);
    void slotNextEffect(double v);
    void slotPrevEffect(double v);
    void slotClear(double v);
    void slotEffectSelector(double v);
    void slotEffectEnabledChanged(bool enabled);
    void slotEffectMetaParameter(double v, bool force = false);

  signals:
    // Signal that whoever is in charge of this EffectSlot should clear this
    // EffectSlot (by deleting the effect from the underlying chain).
    // NOTE(Kshitij) : remove
    void clearEffect(unsigned int iEffectNumber);

    void updated();

  private:
    QString debugString() const {
        return QString("EffectSlot(%1)").arg(m_group);
    }

    const unsigned int m_iEffectNumber;
    const QString m_group;
    UserSettingsPointer m_pConfig;

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
    QList<EffectParameterSlotPointer> m_parameterSlots;
    QList<EffectButtonParameterSlotPointer> m_buttonParameters;

    SoftTakeover* m_pSoftTakeover;

    // NOTE(Kshitij) : START EFFECT
    void sendParameterUpdate();

    EffectsManager* m_pEffectsManager;
    EffectManifestPointer m_pManifest;
    EffectInstantiatorPointer m_pInstantiator;
    EngineEffect* m_pEngineEffect;
    // bool m_bAddedToEngine;
    // bool m_bEnabled;
    // TODO(Kshitij) : rename
    QList<EffectParameter*> m_parameters;
    QMap<QString, EffectParameter*> m_parametersById;
    // NOTE(Kshitij) : END EFFECT

    EngineEffectChain* m_pEngineEffectChain;

    DISALLOW_COPY_AND_ASSIGN(EffectSlot);
};

#endif /* EFFECTSLOT_H */
