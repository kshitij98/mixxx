#ifndef EFFECTCHAINSLOT_H
#define EFFECTCHAINSLOT_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QSignalMapper>

#include "engine/channelhandle.h"
#include "util/class.h"
#include "effects/effectchain.h"

// START EFFECTCHAIN
#include <QDomDocument>

#include "effects/defs.h"
#include "engine/channelhandle.h"
#include "effects/effect.h"
// END EFFECTCHAIN

class ControlObject;
class ControlPushButton;
class ControlEncoder;
class EffectChainSlot;

class EffectsManager;
class EngineEffectRack;
class EngineEffectChain;
class EffectChain;
typedef QSharedPointer<EffectChain> EffectChainPointer;


class EffectChainSlot : public QObject {
    Q_OBJECT
  public:
    EffectChainSlot(EffectRack* pRack,
                    const QString& group,
                    const unsigned int iChainNumber,
                    EffectsManager* pEffectsManager,
                    const QString& id = QString());
    virtual ~EffectChainSlot();

    // Get the ID of the loaded EffectChain
    QString id() const;

    unsigned int numSlots() const;
    EffectSlotPointer addEffectSlot(const QString& group);
    EffectSlotPointer getEffectSlot(unsigned int slotNumber);

    void loadEffectChainToSlot(EffectChainPointer pEffectChain = EffectChainPointer());
    void updateRoutingSwitches();
    EffectChainPointer getEffectChain() const;
    EffectChainPointer getOrCreateEffectChain(EffectsManager* pEffectsManager);

    void registerInputChannel(const ChannelHandleAndGroup& handle_group);

    double getSuperParameter() const;
    void setSuperParameter(double value, bool force = false);
    void setSuperParameterDefaultValue(double value);

    // Unload the loaded EffectChain.
    void clear();

    unsigned int getChainSlotNumber() const;

    const QString& getGroup() const {
        return m_group;
    }

    QDomElement toXml(QDomDocument* doc) const;
    void loadChainSlotFromXml(const QDomElement& effectChainElement);



    // START EffectChain
    void addToEngine(EngineEffectRack* pRack, int iIndex);
    void removeFromEngine(EngineEffectRack* pRack, int iIndex);
    void updateEngineState();

    // The ID of an EffectChain is a unique ID given to it to help associate it
    // with the preset from which it was loaded.
    // const QString& id() const;

    // Whether the chain is enabled (eligible for processing).
    bool enabled() const;
    void setEnabled(bool enabled);

    // Activates EffectChain processing for the provided channel.
    void enableForInputChannel(const ChannelHandleAndGroup& handle_group);
    bool enabledForChannel(const ChannelHandleAndGroup& handle_group) const;
    const QSet<ChannelHandleAndGroup>& enabledChannels() const;
    void disableForInputChannel(const ChannelHandleAndGroup& handle_group);

    EffectChainPointer prototype() const;

    // Get the human-readable name of the EffectChain
    const QString& name() const;
    void setName(const QString& name);

    // Get the human-readable description of the EffectChain
    QString description() const;
    void setDescription(const QString& description);

    double mix() const;
    void setMix(const double& dMix);

    static QString mixModeToString(EffectChainMixMode type) {
        switch (type) {
            case EffectChainMixMode::DrySlashWet:
                return "DRY/WET";
            case EffectChainMixMode::DryPlusWet:
                return "DRY+WET";
            default:
                return "UNKNOWN";
        }
    }
    static EffectChainMixMode mixModeFromString(const QString& typeStr) {
        if (typeStr == "DRY/WET") {
            return EffectChainMixMode::DrySlashWet;
        } else if (typeStr == "DRY+WET") {
            return EffectChainMixMode::DryPlusWet;
        } else {
            return EffectChainMixMode::NumMixModes;
        }
    }

    EffectChainMixMode mixMode() const;
    void setMixMode(EffectChainMixMode type);

    void addEffect(EffectPointer pEffect);
    void replaceEffect(unsigned int effectSlotNumber, EffectPointer pEffect);
    void removeEffect(unsigned int effectSlotNumber);
    void refreshAllEffects();

    const QList<EffectPointer>& effects() const;
    unsigned int numEffects() const;

    EngineEffectChain* getEngineEffectChain();

    static EffectChainPointer createFromXml(EffectsManager* pEffectsManager,
                                      const QDomElement& element);
    static EffectChainPointer clone(EffectChainPointer pChain);
    // END EffectChain

    bool isEmpty() const;
    // EffectRack* rack() const;

  signals:
    // Indicates that the effect pEffect has been loaded into slotNumber of
    // EffectChainSlot chainNumber. pEffect may be an invalid pointer, which
    // indicates that a previously loaded effect was removed from the slot.
    void effectLoaded(EffectPointer pEffect, unsigned int chainNumber,
                      unsigned int slotNumber);

    // Indicates that the given EffectChain was loaded into this
    // EffectChainSlot
    void effectChainLoaded(EffectChainPointer pEffectChain);

    // Signal that whoever is in charge of this EffectChainSlot should load the
    // next EffectChain into it.
    void nextChain(unsigned int iChainSlotNumber,
                   EffectChainPointer pEffectChain);

    // Signal that whoever is in charge of this EffectChainSlot should load the
    // previous EffectChain into it.
    void prevChain(unsigned int iChainSlotNumber,
                   EffectChainPointer pEffectChain);

    // Signal that whoever is in charge of this EffectChainSlot should clear
    // this EffectChain (by removing the chain from this EffectChainSlot).
    void clearChain(unsigned int iChainNumber, EffectChainPointer pEffectChain);

    // Signal that whoever is in charge of this EffectChainSlot should load the
    // next Effect into the specified EffectSlot.
    void nextEffect(unsigned int iChainSlotNumber,
                    unsigned int iEffectSlotNumber,
                    EffectPointer pEffect);

    // Signal that whoever is in charge of this EffectChainSlot should load the
    // previous Effect into the specified EffectSlot.
    void prevEffect(unsigned int iChainSlotNumber,
                    unsigned int iEffectSlotNumber,
                    EffectPointer pEffect);

    // Signal that indicates that the EffectChainSlot has been updated.
    void updated();


  private slots:
    void slotChainEffectChanged(unsigned int effectSlotNumber, bool shouldEmit=true);
    void slotChainNameChanged(const QString& name);
    void slotChainEnabledChanged(bool enabled);
    void slotChainMixChanged(double mix);
    void slotChainMixModeChanged(EffectChainMixMode mixMode);
    void slotChainChannelStatusChanged(const QString& group, bool enabled);

    void slotEffectLoaded(EffectPointer pEffect, unsigned int slotNumber);
    // Clears the effect in the given position in the loaded EffectChain.
    void slotClearEffect(unsigned int iEffectSlotNumber);

    void slotControlClear(double v);
    void slotControlChainEnabled(double v);
    void slotControlChainMix(double v);
    void slotControlChainSuperParameter(double v, bool force = false);
    void slotControlChainMixMode(double v);
    void slotControlChainSelector(double v);
    void slotControlChainNextPreset(double v);
    void slotControlChainPrevPreset(double v);
    void slotChannelStatusChanged(const QString& group);

  private:
    QString debugString() const {
        return QString("EffectChainSlot(%1)").arg(m_group);
    }

    void sendParameterUpdate();

    const unsigned int m_iChainSlotNumber;
    const QString m_group;
    EffectRack* m_pEffectRack;

    EffectChainPointer m_pEffectChain;

    ControlPushButton* m_pControlClear;
    ControlObject* m_pControlNumEffects;
    ControlObject* m_pControlNumEffectSlots;
    ControlObject* m_pControlChainLoaded;
    ControlPushButton* m_pControlChainEnabled;
    ControlObject* m_pControlChainMix;
    ControlObject* m_pControlChainSuperParameter;
    ControlPushButton* m_pControlChainMixMode;
    ControlEncoder* m_pControlChainSelector;
    ControlPushButton* m_pControlChainNextPreset;
    ControlPushButton* m_pControlChainPrevPreset;

    /**
      These COs do not affect how the effects are processed;
      they are defined here for skins and controller mappings to communicate
      with each other. They cannot be defined in skins because they must be present
      when both skins and mappings are loaded, otherwise the skin will
      create a new CO with the same ConfigKey but actually be interacting with a different
      object than the mapping.
    **/
    ControlPushButton* m_pControlChainShowFocus;
    ControlPushButton* m_pControlChainShowParameters;
    ControlPushButton* m_pControlChainFocusedEffect;

    struct ChannelInfo {
        // Takes ownership of pEnabled.
        ChannelInfo(const ChannelHandleAndGroup& handle_group, ControlObject* pEnabled)
                : handle_group(handle_group),
                  pEnabled(pEnabled) {

        }
        ~ChannelInfo() {
            delete pEnabled;
        }
        ChannelHandleAndGroup handle_group;
        ControlObject* pEnabled;
    };
    QMap<QString, ChannelInfo*> m_channelInfoByName;

    QList<EffectSlotPointer> m_slots;
    QSignalMapper m_channelStatusMapper;


    // START EffectChain
    EffectsManager* m_pEffectsManager;
    EffectChainPointer m_pPrototype;

    bool m_bEnabled;
    QString m_id;
    QString m_name;
    QString m_description;
    EffectChainMixMode m_mixMode;
    double m_dMix;

    QSet<ChannelHandleAndGroup> m_enabledInputChannels;
    QList<EffectPointer> m_effects;
    EngineEffectChain* m_pEngineEffectChain;
    bool m_bAddedToEngine;
    // END EffectChain

    DISALLOW_COPY_AND_ASSIGN(EffectChainSlot);
};


#endif /* EFFECTCHAINSLOT_H */
