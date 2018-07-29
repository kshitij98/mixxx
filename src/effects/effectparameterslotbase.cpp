#include <QtDebug>

#include "control/controleffectknob.h"
#include "effects/effectparameterslotbase.h"
#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "effects/effectsmanager.h"

EffectParameterSlotBase::EffectParameterSlotBase(EffectsManager* pEffectsManager,
                                                 const QString& group,
                                                 const unsigned int iParameterSlotNumber)
        : m_pEffectsManager(pEffectsManager),
          m_iParameterSlotNumber(iParameterSlotNumber),
          m_group(group),
          m_pEngineEffect(nullptr),
          m_pManifestParameter(EffectManifestParameterPointer()),
          m_pControlValue(nullptr),
          m_pControlLoaded(nullptr),
          m_pControlType(nullptr),
          m_dChainParameter(0.0) {
}

EffectParameterSlotBase::~EffectParameterSlotBase() {
    m_pEngineEffect = nullptr;
    m_pManifestParameter = EffectManifestParameterPointer();
    delete m_pControlLoaded;
    delete m_pControlType;
}

void EffectParameterSlotBase::updateEngineState() {
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

QString EffectParameterSlotBase::name() const {
    if (m_pManifestParameter) {
        return m_pManifestParameter->name();
    }
    return QString();
}

QString EffectParameterSlotBase::shortName() const {
    if (m_pManifestParameter) {
        return m_pManifestParameter->shortName();
    }
    return QString();
}

QString EffectParameterSlotBase::description() const {
    if (m_pManifestParameter) {
        return m_pManifestParameter->description();
    }
    return tr("No effect loaded.");
}

EffectManifestParameterPointer EffectParameterSlotBase::getManifest() {
    if (m_pManifestParameter) {
        return m_pManifestParameter;
    }
    return EffectManifestParameterPointer();
}

void EffectParameterSlotBase::clear() {
      //qDebug() << debugString() << "clear";
    m_pManifestParameter = EffectManifestParameterPointer();
    m_pEngineEffect = nullptr;
    m_pControlLoaded->forceSet(0.0);
    m_pControlValue->set(0.0);
    m_pControlValue->setDefaultValue(0.0);
    m_pControlType->forceSet(0.0);
    emit(updated());
}

void EffectParameterSlotBase::reload() {
    if (m_pManifestParameter && m_pManifestParameter->showInParameterSlot()) {
        m_pControlLoaded->forceSet(1.0);
    } else {
        m_pControlLoaded->forceSet(0.0);
    }
    emit(updated());
}
