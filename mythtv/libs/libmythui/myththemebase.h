#ifndef MYTHTHEMEBASE_H_
#define MYTHTHEMEBASE_H_

#include "mythuiexp.h"

class MythThemeBasePrivate;

class MUI_PUBLIC MythThemeBase
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

