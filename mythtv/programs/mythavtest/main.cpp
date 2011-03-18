#include <unistd.h>
#include <iostream>

using namespace std;

#include <QString>
#include <QRegExp>
#include <QDir>
#include <QApplication>

#include "tv_play.h"
#include "programinfo.h"
#include "mythcommandlineparser.h"

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythdbcon.h"
#include "compat.h"
#include "dbcheck.h"

// libmythui
#include "mythuihelper.h"
#include "mythmainwindow.h"


int main(int argc, char *argv[])
{
    bool cmdline_err;
    MythCommandLineParser cmdline(
        kCLPOverrideSettings     |
        kCLPWindowed             |
        kCLPNoWindowed           |
#ifdef USING_X11
        kCLPDisplay              |
#endif // USING_X11
        kCLPGeometry             |
        kCLPVerbose              |
        kCLPExtra                |
        kCLPHelp);

    print_verbose_messages |= VB_PLAYBACK | VB_LIBAV;

    for (int argpos = 1; argpos < argc; ++argpos)
    {
        if (cmdline.PreParse(argc, argv, argpos, cmdline_err))
        {
            if (cmdline_err)
                return GENERIC_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
    }

    QApplication a(argc, argv);

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHAVTEST);

    int argpos = 1;
    QString filename = "";

    while (argpos < a.argc())
    {
        if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return GENERIC_EXIT_INVALID_CMDLINE;
            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
        else if (a.argv()[argpos][0] != '-')
        {
            filename = a.argv()[argpos];
        }
        else
        {
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
    }

    if (!cmdline.GetDisplay().isEmpty())
    {
        MythUIHelper::SetX11Display(cmdline.GetDisplay());
    }

    if (!cmdline.GetGeometry().isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(cmdline.GetGeometry());
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    QMap<QString, QString> settingsOverride = cmdline.GetSettingsOverride();
    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(*it));
            gCoreContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    setuid(getuid());

    QString themename = gCoreContext->GetSetting("Theme");
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        QString msg = QString("Fatal Error: Couldn't find theme '%1'.")
            .arg(themename);
        VERBOSE(VB_IMPORTANT, msg);
        return GENERIC_EXIT_NO_THEME;
    }

    GetMythUI()->LoadQtConfig();

#if defined(Q_OS_MACX)
    // Mac OS X doesn't define the AudioOutputDevice setting
#else
    QString auddevice = gCoreContext->GetSetting("AudioOutputDevice");
    if (auddevice.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Audio not configured, you need "
                "to run 'mythfrontend', not 'mythtv'.");
        return GENERIC_EXIT_SETUP_ERROR;
    }
#endif

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();

    TV::InitKeys();

    if (!UpgradeTVDatabaseSchema(false))
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Incorrect database schema.");
        delete gContext;
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    TV *tv = new TV();
    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, "Fatal Error: Could not initialize TV class.");
        return GENERIC_EXIT_NOT_OK;
    }

    if (filename.isEmpty())
    {
        TV::StartTV(NULL, kStartTVNoFlags);
    }
    else
    {
        ProgramInfo pginfo(filename);
        TV::StartTV(&pginfo, kStartTVNoFlags);
    }
    delete gContext;

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
