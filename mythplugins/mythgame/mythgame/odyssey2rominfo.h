#ifndef ODYSSEY2ROMINFO_H_
#define ODYSSEY2ROMINFO_H_

#include <mythtv/mythcontext.h>
#include "rominfo.h"

class Odyssey2RomInfo : public RomInfo
{
  public:
    Odyssey2RomInfo(const RomInfo &lhs) :
        RomInfo(lhs) {}
    virtual ~Odyssey2RomInfo() {}
    
    virtual bool FindImage(QString type, QString* result);
};

#endif // ODYSSEY2ROMINFO_H_
