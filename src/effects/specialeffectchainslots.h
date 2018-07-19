#ifndef SPECIALEFFECTCHAINSLOTS_H
#define SPECIALEFFECTCHAINSLOTS_H

#include "effects/effectchainslot.h"
#include "effects/effectsmanager.h"
#include "effects/effectslot.h"
#include "effects/defs.h"
#include "util/memory.h"

class StandardEffectChainSlot : public EffectChainSlot {
  public:
    StandardEffectChainSlot(unsigned int iChainNumber,
                            EffectsManager* pEffectsManager,
                            const QString& id = QString());
    static QString formatEffectChainSlotGroup(const int iChainNumber);
    static QString formatEffectSlotGroup(const int iChainSlotNumber,
                                         const int iEffectSlotNumber);
};

class OutputEffectChainSlot : public EffectChainSlot {
  public:
    OutputEffectChainSlot(EffectsManager* pEffectsManager);

  private:
    static QString formatEffectChainSlotGroup(const QString& group);
};

class PerGroupEffectChainSlot : public EffectChainSlot {
  public:
    PerGroupEffectChainSlot(const QString& group,
                            const QString& chainSlotGroup,
                            EffectsManager* pEffectsManager);
};

class QuickEffectChainSlot : public PerGroupEffectChainSlot {
  public:
    QuickEffectChainSlot(const QString& group,
                         EffectsManager* pEffectsManager);

    void loadEffect(const unsigned int iEffectSlotNumber,
                    const EffectManifestPointer pManifest,
                    std::unique_ptr<EffectProcessor> pProcessor) override;

    static QString formatEffectChainSlotGroup(const QString& group);
    static QString formatEffectSlotGroup(const QString& group,
                                         const int iEffectSlotNumber = 0);
};

class EqualizerEffectChainSlot : public PerGroupEffectChainSlot {
  public:
    EqualizerEffectChainSlot(const QString& group,
                             EffectsManager* pEffectsManager);

    void loadEffect(const unsigned int iEffectSlotNumber,
                    const EffectManifestPointer pManifest,
                    std::unique_ptr<EffectProcessor> pProcessor) override;

    static QString formatEffectChainSlotGroup(const QString& group);
    static QString formatEffectSlotGroup(const QString& group);

  private:
    void setupLegacyAliasesForGroup(const QString& group);
    std::unique_ptr<ControlObject> m_pCOFilterWaveform;
};

#endif /* SPECIALEFFECTCHAINSLOTS_H */
