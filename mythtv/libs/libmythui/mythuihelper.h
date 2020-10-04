#ifndef MYTHUIHELPERS_H_
#define MYTHUIHELPERS_H_

#include <QStringList>
#include <QString>
#include <QFont>
#include <QMutex>

#include "mythuiexp.h"
#include "mythuithemecache.h"
#include "mythuithemehelper.h"

struct MUI_PUBLIC MythUIMenuCallbacks
{
    void (*exec_program)(const QString &cmd);
    void (*exec_program_tv)(const QString &cmd);
    void (*configplugin)(const QString &cmd);
    void (*plugin)(const QString &cmd);
    void (*eject)();
};

class MUI_PUBLIC MythUIHelper : public MythUIThemeCache, public MythUIThemeHelper
{
  public:
    static MythUIHelper *getMythUI();
    static void destroyMythUI();

    void Init(MythUIMenuCallbacks &cbs);
    void Init();

    MythUIMenuCallbacks *GetMenuCBs();

    void LoadQtConfig();
    bool IsScreenSetup() const;

    void AddCurrentLocation(const QString& location);
    QString RemoveCurrentLocation();
    QString GetCurrentLocation(bool fullPath = false, bool mainStackOnly = true);

  protected:
    Q_DISABLE_COPY(MythUIHelper)
    MythUIHelper() = default;
   ~MythUIHelper() = default;

  private:
    QMutex m_locationLock;
    QStringList m_currentLocation;
    MythUIMenuCallbacks m_callbacks { nullptr,nullptr,nullptr,nullptr,nullptr };
    bool m_screenSetup { false };
};

MUI_PUBLIC MythUIHelper *GetMythUI();
MUI_PUBLIC void DestroyMythUI();

#endif

