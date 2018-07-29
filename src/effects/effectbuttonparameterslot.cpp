#include <QtDebug>

#include "effects/effectslot.h"
#include "effects/effectbuttonparameterslot.h"
#include "control/controleffectknob.h"
#include "effects/effectxmlelements.h"
#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "util/math.h"
#include "util/xml.h"

EffectButtonParameterSlot::EffectButtonParameterSlot(EffectsManager* pEffectsManager,
                                                     const QString& group,
                                                     const unsigned int iParameterSlotNumber)
        : EffectParameterSlotBase(pEffectsManager, group,
                                  iParameterSlotNumber) {
    QString itemPrefix = formatItemPrefix(iParameterSlotNumber);
    m_pControlLoaded = new ControlObject(
            ConfigKey(m_group, itemPrefix + QString("_loaded")));

    m_pControlValue = new ControlPushButton(
            ConfigKey(m_group, itemPrefix));
    connect(m_pControlValue, SIGNAL(valueChanged(double)),
            this, SLOT(updateEngineState()));
    ControlPushButton* pControlValue = static_cast<ControlPushButton*>(m_pControlValue);
    pControlValue->setButtonMode(ControlPushButton::POWERWINDOW);

    m_pControlType = new ControlObject(
            ConfigKey(m_group, itemPrefix + QString("_type")));

    // Read-only controls.
    m_pControlType->setReadOnly();
    m_pControlLoaded->setReadOnly();

    clear();
}

EffectButtonParameterSlot::~EffectButtonParameterSlot() {
    // qDebug() << debugString() << "destroyed";
    // m_pControlLoaded and m_pControlType are deleted by ~EffectParameterSlotBase
    delete m_pControlValue;
}

void EffectButtonParameterSlot::loadManifestParameter(unsigned int iParameterNumber,
        EngineEffect* pEngineEffect, EffectManifestParameterPointer pManifestParameter) {
    // qDebug() << debugString() << "loadManifestParameter" << (pEffectSlot ? pEffectSlot->getManifest().name() : "(null)");
    clear();

    if (pManifestParameter != EffectManifestParameterPointer()) {
        m_pManifestParameter = pManifestParameter;
        m_pEngineEffect = pEngineEffect;
        m_iParameterNumber = iParameterNumber;

        // Set the number of states
        int numStates = math_max(m_pManifestParameter->getSteps().size(), 2);
        ControlPushButton* pControlValue = static_cast<ControlPushButton*>(m_pControlValue);
        pControlValue->setStates(numStates);
        //qDebug() << debugString() << "Loading effect parameter" << m_pManifestParameter->name();
        double dMinimum = m_pManifestParameter->getMinimum();
        double dMinimumLimit = dMinimum; // TODO(rryan) expose limit from EffectParameter
        double dMaximum = m_pManifestParameter->getMaximum();
        double dMaximumLimit = dMaximum; // TODO(rryan) expose limit from EffectParameter
        double dDefault = m_pManifestParameter->getDefault();

        if (dMinimum < dMinimumLimit || dMaximum > dMaximumLimit) {
            qWarning() << debugString() << "WARNING: EffectButtonParameter does not satisfy basic sanity checks.";
        }

        // qDebug() << debugString()
        //         << QString("Val: %1 Min: %2 MinLimit: %3 Max: %4 MaxLimit: %5 Default: %6")
        //         .arg(dValue).arg(dMinimum).arg(dMinimumLimit).arg(dMaximum).arg(dMaximumLimit).arg(dDefault);

        m_pControlValue->set(dDefault);
        m_pControlValue->setDefaultValue(dDefault);
        EffectManifestParameter::ControlHint type = m_pManifestParameter->controlHint();
        // TODO(rryan) expose this from EffectParameter
        m_pControlType->forceSet(static_cast<double>(type));
        // Default loaded parameters to loaded and unlinked
        m_pControlLoaded->forceSet(1.0);

        reload();
    }
    emit(updated());
}

void EffectButtonParameterSlot::setValue(double value) {
    m_pControlValue->set(value);
}

QDomElement EffectButtonParameterSlot::toXml(QDomDocument* doc) const {
    QDomElement parameterElement;
    // if (m_pEffectParameter != nullptr) {
    //     parameterElement = doc->createElement(EffectXml::Parameter);
    //     XmlParse::addElement(*doc, parameterElement,
    //                          EffectXml::ParameterValue,
    //                          QString::number(m_pControlValue->get()));
    // }

    return parameterElement;
}

void EffectButtonParameterSlot::loadParameterSlotFromXml(const QDomElement&
                                                  parameterElement) {
    // if (m_pEffectParameter == nullptr) {
    //     return;
    // }
    // if (!parameterElement.hasChildNodes()) {
    //     m_pControlValue->reset();
    // } else {
    //     bool conversionWorked = false;
    //     double value = XmlParse::selectNodeDouble(parameterElement,
    //                                               EffectXml::ParameterValue,
    //                                               &conversionWorked);
    //     if (conversionWorked) {
    //         // Need to use setParameterFrom(..., nullptr) here to
    //         // trigger valueChanged() signal emission and execute slotValueChanged()
    //         m_pControlValue->setParameterFrom(value, nullptr);
    //     }
    //     // If the conversion failed, the default value is kept.
    // }
}
