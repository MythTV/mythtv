#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <memory>

#include <QString>
#include <QDir>
#include <QMap>
#include <QApplication>
#include <QFileInfo>

#include "mythconfig.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dbutil.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "langsettings.h"
#include "mythtranslation.h"
#include "exitcodes.h"
#include "exitprompt.h"
#include "storagegroupeditor.h"
#include "myththemedmenu.h"
#include "mythuihelper.h"
#include "mythdirs.h"
#include "channelscanner_cli.h"
#include "scanwizardconfig.h"
#include "scaninfo.h"
#include "channelimporter.h"
#include "cardutil.h"
#include "dbcheck.h"
#include "videosource.h"
#include "channeleditor.h"
#include "remoteutil.h"
#include "backendsettings.h"
#include "checksetup.h"
#include "startprompt.h"
#include "mythsystemevent.h"
#include "expertsettingseditor.h"
#include "commandlineparser.h"
#include "profilegroup.h"
#include "signalhandling.h"

using namespace std;

ExitPrompter   *exitPrompt  = NULL;
StartPrompter  *startPrompt = NULL;

static MythThemedMenu *menu;
static QString  logfile;

class CleanupGuard
{
  public:
    typedef void (*CleanupFunc)();

  public:
    CleanupGuard(CleanupFunc cleanFunction) :
        m_cleanFunction(cleanFunction) {}

    ~CleanupGuard()
    {
        m_cleanFunction();
    }

  private:
    CleanupFunc m_cleanFunction;
};

static void cleanup()
{
    DestroyMythMainWindow();

    delete gContext;
    gContext = NULL;

    delete qApp;

    SignalHandler::Done();
}

static void SetupMenuCallback(void* data, QString& selection)
{
    (void)data;

    QString sel = selection.toLower();

    if (sel == "general")
    {
        BackendSettings be;
        be.exec();
    }
    else if (sel == "capture cards")
    {
        CaptureCardEditor cce;
        cce.exec();
    }
    else if (sel == "video sources")
    {
        VideoSourceEditor vse;
        vse.exec();
    }
    else if (sel == "card inputs")
    {
        CardInputEditor cie;
        cie.exec();
    }
    else if (sel == "recording profile")
    {
        ProfileGroupEditor editor;
        editor.exec();
    }
    else if (sel == "channel editor")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        ChannelEditor *chanedit = new ChannelEditor(mainStack);

        if (chanedit->Create())
            mainStack->AddScreen(chanedit);
        else
            delete chanedit;
    }
    else if (sel == "storage groups")
    {
        StorageGroupListEditor sge;
        sge.exec();
    }
    else if (sel == "systemeventeditor")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        MythSystemEventEditor *msee = new MythSystemEventEditor(
                                    mainStack, "System Event Editor");

        if (msee->Create())
            mainStack->AddScreen(msee);
        else
            delete msee;
    }
    else if (sel.startsWith("exiting_app"))
    {
        if (!exitPrompt)
            exitPrompt = new ExitPrompter();
        exitPrompt->handleExit();
    }
    else
        LOG(VB_GENERAL, LOG_ERR, "Unknown menu action: " + selection);
}

static bool RunMenu(QString themedir, QString themename)
{
    QByteArray tmp = themedir.toLocal8Bit();
    menu = new MythThemedMenu(
        QString(tmp.constData()), "setup.xml",
        GetMythMainWindow()->GetMainStack(), "mainmenu", false);

    if (menu->foundTheme())
    {
        menu->setCallback(SetupMenuCallback, gContext);
        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, QString("Couldn't use theme '%1'").arg(themename));
    delete menu;
    menu = NULL;

    return false;
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
static bool resetTheme(QString themedir, const QString badtheme)
{
    QString themename = DEFAULT_UI_THEME;

    if (badtheme == DEFAULT_UI_THEME)
        themename = FALLBACK_UI_THEME;

    LOG(VB_GENERAL, LOG_ERR,
        QString("Overriding broken theme '%1' with '%2'")
                .arg(badtheme).arg(themename));

    gCoreContext->OverrideSettingForSession("Theme", themename);
    themedir = GetMythUI()->FindThemeDir(themename);

    MythTranslation::reload();
    GetMythUI()->LoadQtConfig();
#if CONFIG_DARWIN
    GetMythMainWindow()->Init(QT_PAINTER);
#else
    GetMythMainWindow()->Init();
#endif
    GetMythMainWindow()->ReinitDone();

    return RunMenu(themedir, themename);
}

static int reloadTheme(void)
{
    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    MythTranslation::reload();

    GetMythMainWindow()->SetEffectsEnabled(false);

    GetMythUI()->LoadQtConfig();

    menu->Close();
#if CONFIG_DARWIN
    GetMythMainWindow()->Init(QT_PAINTER);
#else
    GetMythMainWindow()->Init();
#endif

    GetMythMainWindow()->ReinitDone();

    GetMythMainWindow()->SetEffectsEnabled(true);

    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
        return GENERIC_EXIT_NO_THEME;

    return 0;
}

int main(int argc, char *argv[])
{
    QString geometry = QString::null;
    QString display  = QString::null;
    bool    doScan   = false;
    bool    doScanList = false;
    bool    doScanSaveOnly = false;
    bool    scanInteractive = true;
    bool    expertMode = false;
    uint    scanImport = 0;
    bool    scanFTAOnly = false;
    ServiceRequirements scanServiceRequirements = kRequireAV;
    uint    scanCardId = 0;
    QString scanTableName = "atsc-vsb8-us";
    QString scanInputName = "";

    MythTVSetupCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    bool quiet = false, use_display = true;
    if (cmdline.toBool("scan"))
    {
        quiet = true;
        use_display = false;
    }

    CleanupGuard callCleanup(cleanup);

#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(FALSE);
#endif
    new QApplication(argc, argv, use_display);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHTV_SETUP);

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL << SIGRTMIN;
    SignalHandler::Init(signallist);
    signal(SIGHUP, SIG_IGN);
#endif

    if (cmdline.toBool("display"))
        display = cmdline.toString("display");
    if (cmdline.toBool("geometry"))
        geometry = cmdline.toString("geometry");

    int retval;
    QString mask("general");
    if ((retval = cmdline.ConfigureLogging(mask, quiet)) != GENERIC_EXIT_OK)
        return retval;

    if (cmdline.toBool("expert"))
        expertMode = true;
    if (cmdline.toBool("scanlist"))
        doScanList = true;
    if (cmdline.toBool("savescan"))
        doScanSaveOnly = true;
    if (cmdline.toBool("scannoninteractive"))
        scanInteractive = false;

    if (cmdline.toBool("importscan"))
        scanImport = cmdline.toUInt("importscan");
    if (cmdline.toBool("ftaonly"))
        scanFTAOnly = true;
    if (cmdline.toBool("servicetype"))
    {
        scanServiceRequirements = kRequireNothing;
        if (cmdline.toString("servicetype").contains("radio"))
           scanServiceRequirements = kRequireAudio;
        if (cmdline.toString("servicetype").contains("tv"))
           scanServiceRequirements = kRequireAV;
        if (cmdline.toString("servicetype").contains("tv+radio") ||
            cmdline.toString("servicetype").contains("radio+tv"))
                scanServiceRequirements = kRequireAudio;
        if (cmdline.toString("servicetype").contains("all"))
           scanServiceRequirements = kRequireNothing;
    }

    if (cmdline.toBool("scan"))
    {
        scanCardId = cmdline.toUInt("scan");
        doScan = true;
    }
    if (cmdline.toBool("freqtable"))
        scanTableName = cmdline.toString("freqtable");
    if (cmdline.toBool("inputname"))
        scanInputName = cmdline.toString("inputname");

    if (!display.isEmpty())
    {
        MythUIHelper::SetX11Display(display);
    }

    if (!geometry.isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(geometry);
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (!gContext->Init(use_display)) // No Upnp, Prompt for db
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    if (!GetMythDB()->HaveSchema())
    {
        if (!InitializeMythSchema())
            return GENERIC_EXIT_DB_ERROR;
    }

    if (use_display)
    {
        GetMythUI()->LoadQtConfig();

        QString fileprefix = GetConfDir();

        QDir dir(fileprefix);
        if (!dir.exists())
            dir.mkdir(fileprefix);
    }

    if (doScan)
    {
        bool okCardID = scanCardId;

        QStringList inputnames = CardUtil::GetInputNames(scanCardId);
        okCardID &= !inputnames.empty();

        if (scanInputName.isEmpty())
            scanInputName = CardUtil::GetStartInput(scanCardId);

        bool okInputName = inputnames.contains(scanInputName);

        doScan = (okCardID && okInputName);

        if (!okCardID)
        {
            cerr << "You must enter a valid cardid to scan." << endl;
            vector<uint> cardids = CardUtil::GetCardIDs();
            if (cardids.empty())
            {
                cerr << "But no cards have been defined on this host"
                     << endl;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
            cerr << "Valid cards: " << endl;
            for (uint i = 0; i < cardids.size(); i++)
            {
                fprintf(stderr, "%5i: %s %s\n",
                        cardids[i],
                        CardUtil::GetRawCardType(cardids[i])
                        .toAscii().constData(),
                        CardUtil::GetVideoDevice(cardids[i])
                        .toAscii().constData());
            }
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        if (!okInputName)
        {
            cerr << "You must enter a valid input to scan this card."
                 << endl;
            cerr << "Valid inputs: " << endl;
            for (int i = 0; i < inputnames.size(); i++)
            {
                cerr << inputnames[i].toAscii().constData() << endl;
            }
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (doScan)
    {
        int ret = 0;
        int firstBreak   = scanTableName.indexOf("-");
        int secondBreak  = scanTableName.lastIndexOf("-");
        if (!firstBreak || !secondBreak || firstBreak == secondBreak)
        {
            cerr << "Failed to parse the frequence table parameter "
                 << scanTableName.toLocal8Bit().constData() << endl
                 << "Please make sure it is in the format freq_std-"
                    "modulation-country." << endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        QString freq_std = scanTableName.mid(0, firstBreak).toLower();
        QString mod      = scanTableName.mid(
            firstBreak+1, secondBreak-firstBreak-1).toLower();
        QString tbl      = scanTableName.mid(secondBreak+1).toLower();
        uint    inputid  = CardUtil::GetInputID(scanCardId, scanInputName);
        uint    sourceid = CardUtil::GetSourceID(inputid);
        QMap<QString,QString> startChan;
        {
            ChannelScannerCLI scanner(doScanSaveOnly, scanInteractive);
            scanner.Scan(
                (freq_std=="atsc") ?
                ScanTypeSetting::FullScan_ATSC :
                ((freq_std=="dvbt") ?
                 ScanTypeSetting::FullScan_DVBT :
                 ScanTypeSetting::FullScan_ATSC),
                /* cardid    */ scanCardId,
                /* inputname */ scanInputName,
                /* sourceid  */ sourceid,
                /* ignore signal timeout */ false,
                /* follow_nit */            true,
                /* test decryption */       true,
                scanFTAOnly,
                scanServiceRequirements,
                // stuff needed for particular scans
                /* mplexid   */ 0,
                startChan, freq_std, mod, tbl);
            ret = qApp->exec();
        }
        return (ret) ? GENERIC_EXIT_NOT_OK : GENERIC_EXIT_OK;
    }

    if (doScanList)
    {
        vector<ScanInfo> scans = LoadScanList();

        cout<<" scanid cardid sourceid processed        date"<<endl;
        for (uint i = 0; i < scans.size(); i++)
        {
            printf("%5i %6i %8i %8s    %20s\n",
                   scans[i].scanid,   scans[i].cardid,
                   scans[i].sourceid, (scans[i].processed) ? "yes" : "no",
                   scans[i].scandate.toString(Qt::ISODate)
                   .toAscii().constData());
        }
        cout<<endl;

        return GENERIC_EXIT_OK;
    }

    if (scanImport)
    {
        vector<ScanInfo> scans = LoadScanList();
        cout<<"*** SCAN IMPORT START ***"<<endl;
        {
            ScanDTVTransportList list = LoadScan(scanImport);
            ChannelImporter ci(false, true, true, true, false,
                               scanFTAOnly, scanServiceRequirements);
            ci.Process(list);
        }
        cout<<"*** SCAN IMPORT END ***"<<endl;
        return GENERIC_EXIT_OK;
    }

    MythTranslation::load("mythfrontend");

    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    MythMainWindow *mainWindow = GetMythMainWindow();
#if CONFIG_DARWIN
    mainWindow->Init(QT_PAINTER);
#else
    mainWindow->Init();
#endif
    mainWindow->setWindowTitle(QObject::tr("MythTV Setup"));

    // We must reload the translation after a language change and this
    // also means clearing the cached/loaded theme strings, so reload the
    // theme which also triggers a translation reload
    if (LanguageSelection::prompt())
    {
        if (!reloadTheme())
            return GENERIC_EXIT_NO_THEME;
    }

    if (!DBUtil::CheckTimeZoneSupport())
    {
        LOG(VB_GENERAL, LOG_ERR, 
            "MySQL time zone support is missing.  "
            "Please install it and try again.  "
            "See 'mysql_tzinfo_to_sql' for assistance.");
        return GENERIC_EXIT_DB_NOTIMEZONE;
    }

    if (!UpgradeTVDatabaseSchema(true))
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't upgrade database to new schema.");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    // Refresh Global/Main Menu keys after DB update in case there was no DB
    // when they were written originally
    mainWindow->ReloadKeys();

    if (!startPrompt)
        startPrompt = new StartPrompter();
    startPrompt->handleStart();

    // Let the user select buttons, type values, scan for channels, etc.
    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
        return GENERIC_EXIT_NO_THEME;

    ExpertSettingsEditor *expertEditor = NULL;
    if (expertMode)
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        expertEditor =
            new ExpertSettingsEditor(mainStack, "Expert Settings Editor");
        if (expertEditor->Create())
            mainStack->AddScreen(expertEditor);
        else
        {
            delete expertEditor;
            expertEditor = NULL;
            LOG(VB_GENERAL, LOG_ERR,
                "Unable to create expert settings editor window");
            return GENERIC_EXIT_OK;
        }
    }

    qApp->exec();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
