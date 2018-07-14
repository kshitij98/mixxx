#ifndef BUILTINBACKEND_H
#define BUILTINBACKEND_H

#include "effects/defs.h"
#include "effects/effectsbackend.h"

class BuiltInBackend : public EffectsBackend {
  public:
    BuiltInBackend();
    virtual ~BuiltInBackend();

    EffectBackendType getType() const { return EffectBackendType::BuiltIn; };

    const QList<QString> getEffectIds() const;
    EffectManifestPointer getManifest(const QString& effectId) const;
    std::unique_ptr<EffectProcessor> createProcessor(const QString& effectId) const;
    bool canInstantiateEffect(const QString& effectId) const;

  private:
    QString debugString() const {
        return "BuiltInBackend";
    }

    // EffectProcessorInstantiator and RegisteredEffect associate the QString
    // IDs of effects with EffectProcessorImpl<EffectSpecificState> subclasses
    class EffectProcessorInstantiator {
      public:
            virtual ~EffectProcessorInstantiator() {};
            virtual EffectProcessor* instantiate() = 0;
    };
    typedef QSharedPointer<EffectProcessorInstantiator> EffectProcessorInstantiatorPointer;

    template <typename T>
    class EffectProcessorSpecificInstantiator : public EffectProcessorInstantiator {
      public:
        EffectProcessor* instantiate() {
            return new T();
        }
    };

    class RegisteredEffect {
      public:
        RegisteredEffect(EffectManifestPointer pManifest,
                         EffectProcessorInstantiatorPointer pInitator)
            : m_pManifest(pManifest),
              m_pInitator(pInitator) {
        }

        RegisteredEffect() {
        }

        EffectManifestPointer manifest() const { return m_pManifest; };
        EffectProcessorInstantiatorPointer initiator() const { return m_pInitator; };

      private:
        EffectManifestPointer m_pManifest;
        EffectProcessorInstantiatorPointer m_pInitator;
    };

    void registerEffect(const QString& id,
            EffectManifestPointer pManifest,
                        EffectProcessorInstantiatorPointer pInstantiator);

    template <typename EffectProcessorImpl>
    void registerEffect() {
        registerEffect(
                EffectProcessorImpl::getId(),
                EffectProcessorImpl::getManifest(),
                EffectProcessorInstantiatorPointer(
                        new EffectProcessorSpecificInstantiator<EffectProcessorImpl>()));
    }

    QMap<QString, RegisteredEffect> m_registeredEffects;
    QList<QString> m_effectIds;
};

#endif /* BUILTINBACKEND_H */
