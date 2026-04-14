#ifndef MYTHUIHELPERS_H_
#define MYTHUIHELPERS_H_

#include <QStringList>
#include <QString>
#include <QFont>
#include <QMutex>

#include "mythuiexp.h"
#include "mythuithemecache.h"
#include "mythuithemehelper.h"
#include "mythuilocation.h"

struct MUI_PUBLIC MythUIMenuCallbacks
{
    void (*exec_program)(const QString &cmd)    { nullptr };
    void (*exec_program_tv)(const QString &cmd) { nullptr };
    void (*configplugin)(const QString &cmd)    { nullptr };
    void (*plugin)(const QString &cmd)          { nullptr };
    void (*eject)()                             { nullptr };
};

class MUI_PUBLIC MythUIHelper : public MythUIThemeCache, public MythUIThemeHelper,
                                public MythUILocation
{
  public:
    static MythUIHelper *getMythUI();
    static void destroyMythUI();

    void Init(MythUIMenuCallbacks &cbs);
    void Init();

    MythUIMenuCallbacks *GetMenuCBs();

    bool IsScreenSetup() const;

  protected:
    Q_DISABLE_COPY(MythUIHelper)
    MythUIHelper() = default;
   ~MythUIHelper() = default;

  private:
    MythUIMenuCallbacks m_callbacks;
    bool m_screenSetup { false };
};

MUI_PUBLIC MythUIHelper *GetMythUI();
MUI_PUBLIC void DestroyMythUI();

#endif

