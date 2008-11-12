#ifndef MYTHTHEMEBASE_H_
#define MYTHTHEMEBASE_H_

#include "mythexp.h"

class MythThemeBasePrivate;

class MPUBLIC MythThemeBase
{
  public:
    MythThemeBase();
   ~MythThemeBase();

    void Reload(void);

  private:
    void Init(void);

    MythThemeBasePrivate *d;
};

#endif

