#ifndef EFFECTCHAINPRESET_H
#define EFFECTCHAINPRESET_H

// TODO(Kshitij) : Remove redundant header files
#include <QObject>
#include <QMap>
#include <QList>
#include <QDomDocument>

#include "effects/defs.h"
#include "engine/channelhandle.h"
#include "effects/effect.h"
#include "util/class.h"

// Note(Kshitij) : No need to inherit from QObject because of the absence of slots and signals 

class EffectChainPreset {
  public:
    EffectChainPreset();
    EffectChainPreset(QDomElement savedPresetXml);
    ~EffectChainPreset();
 
    double dMix;
    double dSuper;
    EffectChainMixMode mixMode;
 
    QDomElement toXml();
 
  private:
    QList<EffectPreset> m_effectPresets;
};

#endif /* EFFECTCHAINPRESET_H */