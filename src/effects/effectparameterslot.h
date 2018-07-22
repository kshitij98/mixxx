#ifndef EFFECTPARAMETERSLOT_H
#define EFFECTPARAMETERSLOT_H

#include <QObject>
#include <QVariant>
#include <QString>

#include "control/controlobject.h"
#include "effects/effectparameterslotbase.h"
#include "util/class.h"

class ControlObject;
class ControlPushButton;
class ControlEffectKnob;
class SoftTakeover;
class EffectSlot;

class EffectParameterSlot : public EffectParameterSlotBase {
    Q_OBJECT
  public:
    EffectParameterSlot(EffectsManager* pEffectsManager, const QString& group,
            const unsigned int iParameterSlotNumber);
    virtual ~EffectParameterSlot();

    static QString formatItemPrefix(const unsigned int iParameterSlotNumber) {
        return QString("parameter%1").arg(iParameterSlotNumber + 1);
    }

    // Load the parameter of the given effect into this EffectParameterSlot
    void loadManifestParameter(EngineEffect* pEngineEffect,
            EffectManifestParameterPointer pManifestParameter);

    void setValue(double value);

    double getValueParameter() const;

    void onEffectMetaParameterChanged(double parameter, bool force=false);

    // Syncs the Super button with the parameter, that the following
    // super button change will be passed to the effect parameter
    // used during test
    void syncSofttakeover();

    // Clear the currently loaded effect
    void clear();

    QDomElement toXml(QDomDocument* doc) const override;
    void loadParameterSlotFromXml(const QDomElement& parameterElement) override;

  public slots:
    void updateEngineState();

  private slots:
    void slotLinkTypeChanging(double v);
    void slotLinkInverseChanged(double v);

  private:
    QString debugString() const {
        return QString("EffectParameterSlot(%1,%2)").arg(m_group).arg(m_iParameterSlotNumber);
    }

    SoftTakeover* m_pSoftTakeover;

    // Control exposed to the rest of Mixxx
    ControlEffectKnob* m_pControlValue;
    ControlPushButton* m_pControlLinkType;
    ControlPushButton* m_pControlLinkInverse;

    DISALLOW_COPY_AND_ASSIGN(EffectParameterSlot);
};

#endif // EFFECTPARAMETERSLOT_H
