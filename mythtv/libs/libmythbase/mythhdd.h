#ifndef MYTHHDD_H
#define MYTHHDD_H

#include "mythmedia.h"

class MPUBLIC MythHDD : public MythMediaDevice
{
  public:
    MythHDD(QObject* par, const char* DevicePath,
            bool SuperMount, bool AllowEject);

    virtual MythMediaStatus checkMedia(void);

    static MythHDD* Get(QObject* par, const char* devicePath,
                        bool SuperMount, bool AllowEject);
};

#endif
