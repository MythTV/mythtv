#ifndef NESROMINFO_H_
#define NESROMINFO_H_

#include <mythtv/mythcontext.h>
#include "rominfo.h"

class NesRomInfo : public RomInfo
{
  public:
    NesRomInfo(const RomInfo &lhs) :
        RomInfo(lhs) {}
    virtual ~NesRomInfo() {}
    
    virtual bool FindImage(QString type, QString* result);
};

#endif // NESROMINFO_H_
