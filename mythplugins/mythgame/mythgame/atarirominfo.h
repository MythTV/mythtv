#ifndef ATARIROMINFO_H_
#define ATARIROMINFO_H_

#include <mythtv/mythcontext.h>
#include "rominfo.h"

class AtariRomInfo : public RomInfo
{
  public:
    AtariRomInfo(const RomInfo &lhs) :
        RomInfo(lhs) {}
    virtual ~AtariRomInfo() {}
    
    virtual bool FindImage(QString type, QString* result);
};

#endif // ATARIROMINFO_H_
