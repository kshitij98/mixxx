#include <QtDebug>

#include "effects/effectparameterslot.h"

#include "effects/effectslot.h"
#include "control/controleffectknob.h"
#include "effects/effectxmlelements.h"
#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "controllers/softtakeover.h"
#include "util/xml.h"

EffectParameterSlot::EffectParameterSlot(EffectsManager* pEffectsManager,
                                         const QString& group,
                                         const unsigned int iParameterSlotNumber)
        : EffectParameterSlotBase(pEffectsManager, group, iParameterSlotNumber) {
    QString itemPrefix = formatItemPrefix(iParameterSlotNumber);

    m_pControlValue = new ControlEffectKnob(
            ConfigKey(m_group, itemPrefix));
    connect(m_pControlValue, SIGNAL(valueChanged(double)),
            this, SLOT(updateEngineState()));

    m_pControlLoaded = new ControlObject(
            ConfigKey(m_group, itemPrefix + QString("_loaded")));
    m_pControlLoaded->setReadOnly();

    m_pControlType = new ControlObject(
            ConfigKey(m_group, itemPrefix + QString("_type")));
    m_pControlType->setReadOnly();

    m_pControlLinkType = new ControlPushButton(
            ConfigKey(m_group, itemPrefix + QString("_link_type")));
    m_pControlLinkType->setButtonMode(ControlPushButton::TOGGLE);
    m_pControlLinkType->setStates(
        static_cast<double>(EffectManifestParameter::LinkType::NUM_LINK_TYPES));
    m_pControlLinkType->connectValueChangeRequest(
            this, SLOT(slotLinkTypeChanging(double)));

    m_pControlLinkInverse = new ControlPushButton(
            ConfigKey(m_group, itemPrefix + QString("_link_inverse")));
    m_pControlLinkInverse->setButtonMode(ControlPushButton::TOGGLE);
    connect(m_pControlLinkInverse, SIGNAL(valueChanged(double)),
            this, SLOT(slotLinkInverseChanged(double)));

    m_pSoftTakeover = new SoftTakeover();

    clear();
}

EffectParameterSlot::~EffectParameterSlot() {
    // qDebug() << debugString() << "destroyed";
    delete m_pControlValue;
    // m_pControlLoaded and m_pControlType are deleted by ~EffectParameterSlotBase
    delete m_pControlLinkType;
    delete m_pControlLinkInverse;
    delete m_pSoftTakeover;
}

void EffectParameterSlot::updateEngineState() {
    if (!m_pEngineEffect || m_pManifestParameter == EffectManifestParameterPointer()) {
        return;
    }
    EffectsRequest* pRequest = new EffectsRequest();
    pRequest->type = EffectsRequest::SET_PARAMETER_PARAMETERS;
    pRequest->pTargetEffect = m_pEngineEffect;
    pRequest->SetParameterParameters.iParameter = m_iParameterNumber;
    pRequest->value = m_pControlValue->get();
    pRequest->minimum = m_pManifestParameter->getMinimum();
    pRequest->maximum = m_pManifestParameter->getMaximum();
    pRequest->default_value = m_pManifestParameter->getDefault();
    m_pEffectsManager->writeRequest(pRequest);
}

void EffectParameterSlot::loadManifestParameter(unsigned int iParameterNumber,
        EngineEffect* pEngineEffect, EffectManifestParameterPointer pManifestParameter) {
    // qDebug() << debugString() << "loadEffect" << (pManifestParameter ? pManifestParameter->name() : "(null)");
    clear();
    if (pManifestParameter != EffectManifestParameterPointer()) {
        m_pManifestParameter = pManifestParameter;
        m_pEngineEffect = pEngineEffect;
        m_iParameterNumber = iParameterNumber;

        // qDebug() << debugString() << "Loading effect parameter" << m_pManifestParameter->name();
        double dMinimum = m_pManifestParameter->getMinimum();
        double dMinimumLimit = dMinimum; // TODO(rryan) expose limit from EffectParameter
        double dMaximum = m_pManifestParameter->getMaximum();
        double dMaximumLimit = dMaximum; // TODO(rryan) expose limit from EffectParameter
        double dDefault = m_pManifestParameter->getDefault();

        if (dMinimum < dMinimumLimit || dMaximum > dMaximumLimit) {
            qWarning() << debugString() << "WARNING: EffectParameter does not satisfy basic sanity checks.";
        }

        // qDebug() << debugString()
        //          << QString("Val: %1 Min: %2 MinLimit: %3 Max: %4 MaxLimit: %5 Default: %6")
        //          .arg(dValue).arg(dMinimum).arg(dMinimumLimit).arg(dMaximum).arg(dMaximumLimit).arg(dDefault);

        EffectManifestParameter::ControlHint type = m_pManifestParameter->controlHint();
        m_pControlValue->setBehaviour(type, dMinimum, dMaximum);
        m_pControlValue->setDefaultValue(dDefault);
        m_pControlValue->set(dDefault);
        // TODO(rryan) expose this from EffectParameter
        m_pControlType->forceSet(static_cast<double>(type));
        // Default loaded parameters to loaded and unlinked
        m_pControlLoaded->forceSet(1.0);

        m_pControlLinkType->set(
            static_cast<double>(m_pManifestParameter->defaultLinkType()));
        m_pControlLinkInverse->set(
            static_cast<double>(m_pManifestParameter->defaultLinkInversion()));
    }
    emit(updated());
}

void EffectParameterSlot::clear() {
    //qDebug() << debugString() << "clear";
    m_pManifestParameter = EffectManifestParameterPointer();
    m_pEngineEffect = nullptr;
    m_pControlLoaded->forceSet(0.0);
    m_pControlValue->set(0.0);
    m_pControlValue->setDefaultValue(0.0);
    m_pControlType->forceSet(0.0);
    m_pControlLinkType->setAndConfirm(
        static_cast<double>(EffectManifestParameter::LinkType::NONE));
    m_pSoftTakeover->setThreshold(SoftTakeover::kDefaultTakeoverThreshold);
    m_pControlLinkInverse->set(0.0);
    emit(updated());
}

void EffectParameterSlot::setValue(double value) {
    m_pControlValue->set(value);
}

void EffectParameterSlot::slotLinkTypeChanging(double v) {
    m_pSoftTakeover->ignoreNext();
    EffectManifestParameter::LinkType newType =
        static_cast<EffectManifestParameter::LinkType>(
            static_cast<int>(v));
    if (newType == EffectManifestParameter::LinkType::LINKED_LEFT ||
        newType == EffectManifestParameter::LinkType::LINKED_RIGHT ||
        newType == EffectManifestParameter::LinkType::LINKED_LEFT_RIGHT) {
        double neutral = m_pManifestParameter->neutralPointOnScale();
        if (neutral > 0.0 && neutral < 1.0) {
            // Knob is already a split knob, meaning it has a positive and
            // negative effect if it's twisted above the neutral point or
            // below the neutral point.
            // Toggle back to 0
            newType = EffectManifestParameter::LinkType::NONE;
        }
    }
    if (newType == EffectManifestParameter::LinkType::LINKED_LEFT ||
        newType == EffectManifestParameter::LinkType::LINKED_RIGHT) {
        m_pSoftTakeover->setThreshold(
                SoftTakeover::kDefaultTakeoverThreshold * 2.0);
    } else {
        m_pSoftTakeover->setThreshold(SoftTakeover::kDefaultTakeoverThreshold);
    }
    m_pControlLinkType->setAndConfirm(static_cast<double>(newType));
}

void EffectParameterSlot::slotLinkInverseChanged(double v) {
    Q_UNUSED(v);
    m_pSoftTakeover->ignoreNext();
}

void EffectParameterSlot::onEffectMetaParameterChanged(double parameter, bool force) {
    m_dChainParameter = parameter;
    if (m_pManifestParameter != EffectManifestParameterPointer()) {
        // Intermediate cast to integer is needed for VC++.
        EffectManifestParameter::LinkType type =
                static_cast<EffectManifestParameter::LinkType>(
                        static_cast<int>(m_pControlLinkType->get()));

        bool inverse = m_pControlLinkInverse->toBool();

        switch (type) {
            case EffectManifestParameter::LinkType::LINKED:
                if (parameter < 0.0 || parameter > 1.0) {
                    return;
                }
                {
                    double neutral = m_pManifestParameter->neutralPointOnScale();
                    if (neutral > 0.0 && neutral < 1.0) {
                        if (inverse) {
                            // the neutral position must stick where it is
                            neutral = 1.0 - neutral;
                        }
                        // Knob is already a split knob
                        // Match to center position of meta knob
                        if (parameter <= 0.5) {
                            parameter /= 0.5;
                            parameter *= neutral;
                        } else {
                            parameter -= 0.5;
                            parameter /= 0.5;
                            parameter *= 1 - neutral;
                            parameter += neutral;
                        }
                    }
                }
                break;
            case EffectManifestParameter::LinkType::LINKED_LEFT:
                if (parameter >= 0.5 && parameter <= 1.0) {
                    parameter = 1;
                } else if (parameter >= 0.0 && parameter <= 0.5) {
                    parameter *= 2;
                } else {
                    return;
                }
                break;
            case EffectManifestParameter::LinkType::LINKED_RIGHT:
                if (parameter >= 0.5 && parameter <= 1.0) {
                    parameter -= 0.5;
                    parameter *= 2;
                } else if (parameter >= 0.0 && parameter < 0.5) {
                    parameter = 0.0;
                } else {
                    return;
                }
                break;
            case EffectManifestParameter::LinkType::LINKED_LEFT_RIGHT:
                if (parameter >= 0.5 && parameter <= 1.0) {
                    parameter -= 0.5;
                    parameter *= 2;
                } else if (parameter >= 0.0 && parameter < 0.5) {
                    parameter *= 2;
                    parameter = 1.0 - parameter;
                } else {
                    return;
                }
                break;
            case EffectManifestParameter::LinkType::NONE:
            default:
                return;
        }

        if (inverse) {
            parameter = 1.0 - parameter;
        }

        //qDebug() << "onEffectMetaParameterChanged" << debugString() << parameter << "force?" << force;
        if (force) {
            m_pControlValue->setParameterFrom(parameter, NULL);
            // This ensures that softtakover is in sync for following updates
            m_pSoftTakeover->ignore(m_pControlValue, parameter);
        } else if (!m_pSoftTakeover->ignore(m_pControlValue, parameter)) {
            m_pControlValue->setParameterFrom(parameter, NULL);
        }
    }
}

void EffectParameterSlot::syncSofttakeover() {
    double parameter = m_pControlValue->getParameter();
    m_pSoftTakeover->ignore(m_pControlValue, parameter);
}

double EffectParameterSlot::getValueParameter() const {
    return m_pControlValue->getParameter();
}

QDomElement EffectParameterSlot::toXml(QDomDocument* doc) const {
    QDomElement parameterElement;
    // if (m_pEffectParameter != nullptr) {
    //     parameterElement = doc->createElement(EffectXml::Parameter);
    //     XmlParse::addElement(*doc, parameterElement,
    //                          EffectXml::ParameterValue,
    //                          QString::number(m_pControlValue->getParameter()));
    //     XmlParse::addElement(*doc, parameterElement,
    //                          EffectXml::ParameterLinkType,
    //                          EffectManifestParameter::LinkTypeToString(
    //                             static_cast<EffectManifestParameter::LinkType>(
    //                                 static_cast<int>(m_pControlLinkType->get()))));
    //     XmlParse::addElement(*doc, parameterElement,
    //                          EffectXml::ParameterLinkInversion,
    //                          QString::number(m_pControlLinkInverse->get()));
    // }

    return parameterElement;
}

void EffectParameterSlot::loadParameterSlotFromXml(const QDomElement& parameterElement) {
    // if (m_pEffectParameter == nullptr) {
    //     return;
    // }
    // if (!parameterElement.hasChildNodes()) {
    //     m_pControlValue->reset();
    //     m_pControlLinkType->set(
    //         static_cast<double>(m_pEffectParameter->getDefaultLinkType()));
    //     m_pControlLinkInverse->set(
    //         static_cast<double>(m_pEffectParameter->getDefaultLinkInversion()));
    // } else {
    //     // Need to use setParameterFrom(..., nullptr) here to
    //     // trigger valueChanged() signal emission and execute slotValueChanged()
    //     bool conversionWorked = false;
    //     double value = XmlParse::selectNodeDouble(parameterElement,
    //                        EffectXml::ParameterValue, &conversionWorked);
    //     if (conversionWorked) {
    //         // Need to use setParameterFrom(..., nullptr) here to
    //         // trigger valueChanged() signal emission and execute slotValueChanged()
    //         m_pControlValue->setParameterFrom(value, nullptr);
    //     }
    //     // If the conversion failed, the default value is kept.

    //     QString linkTypeString = XmlParse::selectNodeQString(parameterElement,
    //                                  EffectXml::ParameterLinkType);
    //     if (!linkTypeString.isEmpty()) {
    //         m_pControlLinkType->set(static_cast<double>(
    //             EffectManifestParameter::LinkTypeFromString(linkTypeString)));
    //     }

    //     conversionWorked = false;
    //     double linkInversion = XmlParse::selectNodeDouble(parameterElement,
    //                                EffectXml::ParameterLinkInversion, &conversionWorked);
    //     if (conversionWorked) {
    //         m_pControlLinkInverse->set(linkInversion);
    //     }
    // }
}
