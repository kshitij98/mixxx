#ifndef EFFECTPARAMETERSLOTBASE_H
#define EFFECTPARAMETERSLOTBASE_H

#include <QObject>
#include <QVariant>
#include <QString>

#include "control/controlobject.h"
#include "effects/effectmanifest.h"
#include "engine/effects/message.h"
#include "util/class.h"

class ControlObject;
class ControlPushButton;
class EffectParameter;
class EffectSlot;
class EngineEffect;
class EffectsManager;

class EffectParameterSlotBase : public QObject {
    Q_OBJECT
  public:
    EffectParameterSlotBase(EffectsManager* pEffectsManager, const QString& group,
            const unsigned int iParameterSlotNumber);
    virtual ~EffectParameterSlotBase();

    const bool isLoaded() const {
        return m_pEngineEffect != nullptr;
    }
    QString name() const;
    QString shortName() const;
    QString description() const;
    EffectManifestParameterPointer getManifest();

    // Clear the currently loaded effect
    void clear();

    virtual QDomElement toXml(QDomDocument* doc) const = 0;
    virtual void loadParameterSlotFromXml(const QDomElement& parameterElement) = 0;

  signals:
    // Signal that indicates that the EffectParameterSlotBase has been updated.
    void updated();

  public slots:
    void updateEngineState();

  protected:
    EffectsManager* m_pEffectsManager;
    const unsigned int m_iParameterSlotNumber;
    unsigned int m_iParameterNumber;
    QString m_group;
    EngineEffect* m_pEngineEffect;
    EffectManifestParameterPointer m_pManifestParameter;

    // Controls exposed to the rest of Mixxx
    ControlObject* m_pControlValue;
    ControlObject* m_pControlLoaded;
    ControlObject* m_pControlType;
    double m_dChainParameter;

    DISALLOW_COPY_AND_ASSIGN(EffectParameterSlotBase);
};

#endif /* EFFECTPARAMETERSLOTBASE_H */
