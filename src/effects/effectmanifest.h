#ifndef EFFECTMANIFEST_H
#define EFFECTMANIFEST_H

#include <QList>
#include <QString>
#include <QtDebug>
#include <QSharedPointer>

#include "effects/effectmanifestparameter.h"
#include "effects/defs.h"

// An EffectManifest is a full description of the metadata associated with an
// effect (e.g. name, author, version, description, etc.) and the parameters of
// the effect that are intended to be exposed to the rest of Mixxx for user or
// script control.
//
// EffectManifest is composed purely of simple data types, and when an
// EffectManifest is const, it should be completely immutable. EffectManifest is
// meant to be used in most cases as a reference, and in Qt collections, so it
// is important that the implicit copy and assign constructors work, and that
// the no-argument constructor be non-explicit.
class EffectManifest {
  public:
    EffectManifest()
        : m_backendType(EffectBackendType::Unknown),
          m_isMixingEQ(false),
          m_isMasterEQ(false),
          m_effectRampsFromDry(false),
          m_bAddDryToWet(false),
          m_metaknobDefault(0.5) {
    }

    // Hack to store unique IDs in QComboBox models
    const QString uniqueId() const {
        return m_id + " " + backendName();
    }

    // WARNING! Effects must not be identified solely by ID string or name.
    // ID strings and names are only unique among EffectManifests from one
    // EffectsBackend. Use EffectManifest::operator== to compare both ID string
    // and EffectBackendType.
    const QString& id() const {
        return m_id;
    }
    void setId(const QString& id) {
        m_id = id;
    }

    const QString& name() const {
        return m_name;
    }
    void setName(const QString& name) {
        m_name = name;
    }

    const QString& shortName() const {
        return m_shortName;
    }
    void setShortName(const QString& shortName) {
        m_shortName = shortName;
    }

    const QString& displayName() const {
        if (!m_shortName.isEmpty()) {
            return m_shortName;
        } else {
            return m_name;
        }
    }

    const EffectBackendType& backendType() const {
        return m_backendType;
    }
    void setBackendType(const EffectBackendType& type) {
        m_backendType = type;
    }

    const QString& author() const {
        return m_author;
    }
    void setAuthor(const QString& author) {
        m_author = author;
    }

    const QString& version() const {
        return m_version;
    }
    void setVersion(const QString& version) {
        m_version = version;
    }

    const QString& description() const {
        return m_description;
    }

    const bool& isMixingEQ() const {
        return m_isMixingEQ;
    }

    void setIsMixingEQ(const bool value) {
        m_isMixingEQ = value;
    }

    const bool& isMasterEQ() const {
        return m_isMasterEQ;
    }

    void setIsMasterEQ(const bool value) {
        m_isMasterEQ = value;
    }

    void setDescription(const QString& description) {
        m_description = description;
    }

    const QList<EffectManifestParameterPointer>& parameters() const {
        return m_parameters;
    }

    EffectManifestParameterPointer addParameter() {
        EffectManifestParameterPointer pManifestParameter(
                new EffectManifestParameter());
        m_parameters.append(pManifestParameter);
        return pManifestParameter;
    }

    EffectManifestParameterPointer parameter(int i) {
        return m_parameters[i];
    }

    unsigned int numKnobParameters() const {
        unsigned int num = 0;
        for (const auto& pManifestParameter : m_parameters) {
            if (pManifestParameter->controlHint() !=
                    EffectManifestParameter::ControlHint::TOGGLE_STEPPING) {
                ++num;
            }
        }
        return num;
    }

    unsigned int numButtonParameters() const {
        unsigned int num = 0;
        for (const auto& pManifestParameter : m_parameters) {
            if (pManifestParameter->controlHint() ==
                    EffectManifestParameter::ControlHint::TOGGLE_STEPPING) {
                ++num;
            }
        }
        return num;
    }

    bool effectRampsFromDry() const {
        return m_effectRampsFromDry;
    }
    void setEffectRampsFromDry(bool effectFadesFromDry) {
        m_effectRampsFromDry = effectFadesFromDry;
    }

    bool addDryToWet() const {
        return m_bAddDryToWet;
    }
    void setAddDryToWet(bool addDryToWet) {
        m_bAddDryToWet = addDryToWet;
    }

    double metaknobDefault() const {
        return m_metaknobDefault;
    }
    void setMetaknobDefault(double metaknobDefault) {
        m_metaknobDefault = metaknobDefault;
    }

    QString backendName() const {
        switch (m_backendType) {
            case EffectBackendType::BuiltIn:
                return QString("Built-in");
            case EffectBackendType::LV2:
                return QString("LV2");
            default:
                return QString("Unknown");
        }
    }

    // Use this when showing the string in the GUI
    QString translatedBackendName() const {
        switch (m_backendType) {
            case EffectBackendType::BuiltIn:
                //: Used for effects that are built into Mixxx
                return QObject::tr("Built-in");
            case EffectBackendType::LV2:
                return QString("LV2");
            default:
                return QString();
        }
    }
    static EffectBackendType backendTypeFromString(const QString& name) {
        if (name == "Built-in") {
            return EffectBackendType::BuiltIn;
        } else if (name == "LV2") {
            return EffectBackendType::LV2;
        } else {
            return EffectBackendType::Unknown;
        }
    }

    bool operator==(const EffectManifest& other) const {
        return other.id() == m_id && other.backendType() == m_backendType;
    }

    bool operator<(const EffectManifest& other) const {
        if (other.backendType() != m_backendType) {
            return other.backendType() < m_backendType;
        }
        return other.id() < m_id;
    }

  private:
    QString debugString() const {
        return QString("EffectManifest(%1)").arg(m_id);
    }

    QString m_id;
    QString m_name;
    QString m_shortName;
    EffectBackendType m_backendType;
    QString m_author;
    QString m_version;
    QString m_description;
    // This helps us at DlgPrefEQ's basic selection of Equalizers
    bool m_isMixingEQ;
    bool m_isMasterEQ;
    QList<EffectManifestParameterPointer> m_parameters;
    bool m_effectRampsFromDry;
    bool m_bAddDryToWet;
    double m_metaknobDefault;
};

#endif /* EFFECTMANIFEST_H */
