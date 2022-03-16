#include "mythuihelper.h"

#include <cmath>
#include <unistd.h>
#include <iostream>

#include <QMutex>
#include <QMap>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QTimer>

// mythbase headers
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/storagegroup.h"

// mythui headers
#include "mythprogressdialog.h"
#include "mythimage.h"
#include "mythmainwindow.h"
#include "themeinfo.h"
#include "x11colors.h"
#include "mythdisplay.h"

#define LOC QString("MythUIHelper: ")

static MythUIHelper *mythui = nullptr;
static QMutex uiLock;

MythUIHelper *MythUIHelper::getMythUI()
{
    if (mythui)
        return mythui;

    uiLock.lock();

    if (!mythui)
        mythui = new MythUIHelper();

    uiLock.unlock();

    // These directories should always exist.  Don't test first as
    // there's no harm in trying to create an existing directory.
    QDir dir;
    dir.mkdir(GetThemeBaseCacheDir());
    dir.mkdir(GetRemoteCacheDir());
    dir.mkdir(GetThumbnailDir());

    return mythui;
}

void MythUIHelper::destroyMythUI()
{
    uiLock.lock();
    delete mythui;
    mythui = nullptr;
    uiLock.unlock();
}

MythUIHelper *GetMythUI()
{
    return MythUIHelper::getMythUI();
}

void DestroyMythUI()
{
    MythUIHelper::destroyMythUI();
}

void MythUIHelper::Init(MythUIMenuCallbacks &cbs)
{
    m_screenSetup = true;
    InitThemeHelper();
    m_callbacks = cbs;
}

// This init is used for showing the startup UI that is shown
// before the database is initialized. The above init is called later,
// after the DB is available.
// This class does not mind being Initialized twice.
void MythUIHelper::Init()
{
    m_screenSetup = true;
    InitThemeHelper();
}

MythUIMenuCallbacks *MythUIHelper::GetMenuCBs()
{
    return &m_callbacks;
}

bool MythUIHelper::IsScreenSetup() const
{
    return m_screenSetup;
}
