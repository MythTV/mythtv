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
#include "loggingserver.h"
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
#include "mythmiscutil.h"
#include "ssdp.h"
#if CONFIG_DARWIN
#include "mythuidefines.h"
#endif
#include "cleanupguard.h"

using namespace std;

ExitPrompter   *exitPrompt  = nullptr;
StartPrompter  *startPrompt = nullptr;

static MythThemedMenu *menu;
static QString  logfile;

static void cleanup()
{
    DestroyMythMainWindow();

    delete gContext;
    gContext = nullptr;

    delete qApp;

    SignalHandler::Done();
}

static void SetupMenuCallback(void* /* data */, QString& selection)
{
    QString sel = selection.toLower();

    if (sel == "general")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new StandardSettingDialog(mainStack, "generalsettings",
                                      new BackendSettings());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "capture cards")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new StandardSettingDialog(mainStack, "capturecardeditor",
                                      new CaptureCardEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "video sources")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new StandardSettingDialog(mainStack, "videosourceeditor",
               new VideoSourceEditor());
        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "card inputs")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new StandardSettingDialog(mainStack, "cardinputeditor",
                                      new CardInputEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "recording profile")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new StandardSettingDialog(mainStack, "recordingprofileeditor",
                                      new ProfileGroupEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
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
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        StandardSettingDialog *ssd =
            new StandardSettingDialog(mainStack, "storagegroupeditor",
                                      new StorageGroupListEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
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
    else if (sel.startsWith("exiting_app") || (sel == "standby_mode"))
    {
        if (!exitPrompt)
            exitPrompt = new ExitPrompter();
        exitPrompt->handleExit();
    }
    else
        LOG(VB_GENERAL, LOG_ERR, "Unknown menu action: " + selection);
}

static bool RunMenu(const QString& themedir, const QString& themename)
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
    menu = nullptr;

    return false;
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
static bool resetTheme(QString themedir, const QString &badtheme)
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

    if (menu)
    {
        menu->Close();
    }
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
    QString geometry;
    QString display;
    bool    doScan   = false;
    bool    doScanList = false;
    bool    doScanSaveOnly = false;
    bool    scanInteractive = true;
    bool    expertMode = false;
    uint    scanImport = 0;
    bool    scanFTAOnly = false;
    bool    scanLCNOnly = false;
    bool    scanCompleteOnly = false;
    bool    addFullTS = false;
    ServiceRequirements scanServiceRequirements = kRequireAV;
    uint    scanCardId = 0;
    QString frequencyStandard = "atsc";
    QString modulation = "vsb8";
    QString region = "us";
    QString scanInputName = "";

#if CONFIG_OMX_RPI
    setenv("QT_XCB_GL_INTEGRATION","none",0);
#endif

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

    if (use_display)
    {

#ifdef Q_OS_MAC
        // Without this, we can't set focus to any of the CheckBoxSetting, and most
        // of the MythPushButton widgets, and they don't use the themed background.
        QApplication::setDesktopSettingsAware(false);
#endif
        new QApplication(argc, argv);
    }
    else {
        new QCoreApplication(argc, argv);
    }
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHTV_SETUP);

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    SignalHandler::SetHandler(SIGHUP, logSigHup);
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
    if (cmdline.toBool("lcnonly"))
        scanLCNOnly = true;
    if (cmdline.toBool("completeonly"))
        scanCompleteOnly = true;
    if (cmdline.toBool("addfullts"))
        addFullTS = true;
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

    if (!cmdline.toBool("noupnp"))
    {
        // start looking for any uPnP devices we can use like VBoxes
        SSDP::Instance()->PerformSearch("ssdp:all");
    }
    if (cmdline.toBool("scan"))
    {
        scanCardId = cmdline.toUInt("scan");
        doScan = true;
    }
    if (cmdline.toBool("freqstd"))
        frequencyStandard = cmdline.toString("freqstd").toLower();
    if (cmdline.toBool("modulation"))
        modulation = cmdline.toString("modulation").toLower();
    if (cmdline.toBool("region"))
        region = cmdline.toString("region").toLower();
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

    cmdline.ApplySettingsOverride();
    if (!gContext->Init(use_display,false,true)) // No Upnp, Prompt for db
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

    setHttpProxy();

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
        bool okCardID = scanCardId != 0U;

        if (scanInputName.isEmpty())
            scanInputName = CardUtil::GetInputName(scanCardId);

        bool okInputName = (scanInputName == CardUtil::GetInputName(scanCardId)
                            && scanInputName != "None");

        doScan = (okCardID && okInputName);

        if (!okCardID)
        {
            cerr << "You must enter a valid cardid to scan." << endl;
            vector<uint> cardids = CardUtil::GetInputIDs();
            if (cardids.empty())
            {
                cerr << "But no cards have been defined on this host"
                     << endl;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
            cerr << "Valid cards: " << endl;
            for (size_t i = 0; i < cardids.size(); i++)
            {
                fprintf(stderr, "%5u: %s %s\n",
                        cardids[i],
                        CardUtil::GetRawInputType(cardids[i])
                        .toLatin1().constData(),
                        CardUtil::GetVideoDevice(cardids[i])
                        .toLatin1().constData());
            }
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        if (!okInputName)
        {
            cerr << "You must enter a valid input to scan this card."
                 << endl;
            cerr << "Valid input: "
                 << CardUtil::GetInputName(scanCardId).toLatin1().constData()
                 << endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    if (doScan)
    {
        int ret = 0;
        uint    inputid  = scanCardId;
        uint    sourceid = CardUtil::GetSourceID(inputid);
        QMap<QString,QString> startChan;
        {
            ChannelScannerCLI scanner(doScanSaveOnly, scanInteractive);

            int scantype;
            if (frequencyStandard == "atsc")
                scantype = ScanTypeSetting::FullScan_ATSC;
            else if (frequencyStandard == "dvbt")
                scantype = ScanTypeSetting::FullScan_DVBT;
            else if (frequencyStandard == "mpeg")
                scantype = ScanTypeSetting::CurrentTransportScan;
            else if (frequencyStandard == "iptv")
            {
                scantype = ScanTypeSetting::IPTVImportMPTS;
                scanner.ImportM3U(scanCardId, scanInputName, sourceid, true);
            }
            else if (frequencyStandard == "extern")
                scantype = ScanTypeSetting::ExternRecImport;
            else
                scantype = ScanTypeSetting::FullScan_ATSC;

            scanner.Scan(scantype,
                         /* cardid    */ scanCardId,
                         /* inputname */ scanInputName,
                         /* sourceid  */ sourceid,
                         /* ignore signal timeout */ false,
                         /* follow_nit */            true,
                         /* test decryption */       true,
                         scanFTAOnly,
                         scanLCNOnly,
                         scanCompleteOnly,
                         addFullTS,
                         scanServiceRequirements,
                         // stuff needed for particular scans
                         /* mplexid   */ 0,
                         startChan, frequencyStandard, modulation, region);
            ret = qApp->exec();
        }
        return (ret) ? GENERIC_EXIT_NOT_OK : GENERIC_EXIT_OK;
    }

    if (doScanList)
    {
        vector<ScanInfo> scans = LoadScanList();

        cout<<" scanid cardid sourceid processed        date"<<endl;
        for (size_t i = 0; i < scans.size(); i++)
        {
            printf("%5i %6i %8i %8s    %20s\n",
                   scans[i].m_scanid,   scans[i].m_cardid,
                   scans[i].m_sourceid, (scans[i].m_processed) ? "yes" : "no",
                   scans[i].m_scandate.toString(Qt::ISODate)
                   .toLatin1().constData());
        }
        cout<<endl;

        return GENERIC_EXIT_OK;
    }

    if (scanImport)
    {
        cout<<"*** SCAN IMPORT START ***"<<endl;
        {
            ScanDTVTransportList list = LoadScan(scanImport);
            ChannelImporter ci(false, true, true, true, false,
                               scanFTAOnly, scanLCNOnly, scanCompleteOnly, scanServiceRequirements);
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

    ExpertSettingsEditor *expertEditor = nullptr;
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
            expertEditor = nullptr;
            LOG(VB_GENERAL, LOG_ERR,
                "Unable to create expert settings editor window");
            return GENERIC_EXIT_OK;
        }
    }

    qApp->exec();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
