#ifndef PCROMINFO_H_
#define PCROMINFO_H_

#include <qstring.h>
#include <mythtv/mythcontext.h>
#include "rominfo.h"

class PCRomInfo : public RomInfo
{
  public:
    PCRomInfo(QString lromname = "",
                QString lsystem = "",
                QString lgamename ="",
                QString lgenre = "",
                int lyear = 0) :
            RomInfo(lromname, lsystem, lgamename, lgenre, lyear)
            {}
            
    PCRomInfo(const RomInfo &lhs) :
                RomInfo(lhs) {}

    virtual ~PCRomInfo() {}

    virtual bool FindImage(QString type,QString *result);
};

#endif
