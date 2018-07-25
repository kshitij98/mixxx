#ifndef EFFECTBUTTONPARAMETERSLOT_H
#define EFFECTBUTTONPARAMETERSLOT_H

#include <QObject>
#include <QVariant>
#include <QString>

#include "control/controlobject.h"
#include "effects/effectparameterslotbase.h"
#include "util/class.h"

class ControlObject;
class ControlPushButton;

class EffectButtonParameterSlot : public EffectParameterSlotBase {
    Q_OBJECT
  public:
    EffectButtonParameterSlot(EffectsManager* pEffectsManager, const QString& group,
            const unsigned int iParameterSlotNumber);
    virtual ~EffectButtonParameterSlot();

    static QString formatItemPrefix(const unsigned int iParameterSlotNumber) {
        return QString("button_parameter%1").arg(iParameterSlotNumber + 1);
    }

    // Load the parameter of the given effect into this EffectButtonParameterSlot
    void loadManifestParameter(unsigned int iParameterNumber, EngineEffect* pEngineEffect,
            EffectManifestParameterPointer pManifestParameter);

    void setValue(double value);

    QDomElement toXml(QDomDocument* doc) const override;
    void loadParameterSlotFromXml(const QDomElement& parameterElement) override;

  private:
    QString debugString() const {
        return QString("EffectButtonParameterSlot(%1,%2)").arg(m_group).arg(m_iParameterSlotNumber);
    }

    DISALLOW_COPY_AND_ASSIGN(EffectButtonParameterSlot);
};

#endif // EFFECTBUTTONPARAMETERSLOT_H
