#ifndef EFFECTPRESET_H
#define EFFECTPRESET_H

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

class EffectPreset {
  public:
    EffectPreset();
    EffectPreset(QDomElement savedPresetXml);
    ~EffectPreset();
 
    double dMix;
    double dSuper;
    EffectChainMixMode mixMode;
 
    QDomElement toXml();
};

#endif /* EFFECTPRESET_H */