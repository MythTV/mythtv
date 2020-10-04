#ifndef MYTHTHEMEBASE_H_
#define MYTHTHEMEBASE_H_

// MythTV
#include "mythuiexp.h"

class MythScreenStack;
class MythScreenType;
class MythMainWindow;

class MUI_PUBLIC MythThemeBase
{
  public:
    explicit MythThemeBase(MythMainWindow* MainWindow);
   ~MythThemeBase();

    void Reload();

  private:
    Q_DISABLE_COPY(MythThemeBase)
    MythScreenStack *m_background       { nullptr };
    MythScreenType  *m_backgroundscreen { nullptr };
};

#endif

