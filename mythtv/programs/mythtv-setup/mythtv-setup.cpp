// C/C++
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <unistd.h>

// Qt
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QString>
#include <QtGlobal>

// MythTV
#include "libmythui/langsettings.h"
#include "libmyth/mythcontext.h"
#include "libmythui/storagegroupeditor.h"
#include "libmythbase/cleanupguard.h"
#include "libmythbase/dbutil.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/remoteutil.h"
#include "libmythbase/signalhandling.h"
#include "libmythtv/cardutil.h"
#include "libmythtv/channelscan/channelimporter.h"
#include "libmythtv/channelscan/channelscanner_cli.h"
#include "libmythtv/channelscan/scaninfo.h"
#include "libmythtv/channelscan/scanwizardconfig.h"
#include "libmythtv/dbcheck.h"
#include "libmythtv/mythsystemevent.h"
#include "libmythtv/profilegroup.h"
#include "libmythtv/videosource.h"
#include "libmythtv/channelgroup.h"
#include "libmythui/mythdisplay.h"
#include "libmythui/myththemedmenu.h"
#include "libmythui/mythuihelper.h"
#include "libmythupnp/ssdp.h"

// MythTV Setup
#include "backendsettings.h"
#include "channeleditor.h"
#include "checksetup.h"
#include "exitprompt.h"
#include "expertsettingseditor.h"
#include "mythtv-setup_commandlineparser.h"
#include "startprompt.h"

ExitPrompter   *exitPrompt  = nullptr;
StartPrompter  *startPrompt = nullptr;

static MythThemedMenu *menu;
static QString  logfile;

static void cleanup()
{
    DestroyMythMainWindow();

    delete gContext;
    gContext = nullptr;

    SignalHandler::Done();
}

static void SetupMenuCallback(void* /* data */, QString& selection)
{
    QString sel = selection.toLower();

    if (sel == "general")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "generalsettings",
                                              new BackendSettings());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "capture cards")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "capturecardeditor",
                                              new CaptureCardEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "video sources")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "videosourceeditor",
               new VideoSourceEditor());
        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "card inputs")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "cardinputeditor",
                                              new CardInputEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "recording profile")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "recordingprofileeditor",
                                              new ProfileGroupEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "channel editor")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        auto *chanedit = new ChannelEditor(mainStack);

        if (chanedit->Create())
            mainStack->AddScreen(chanedit);
        else
            delete chanedit;
    }
    else if (sel == "storage groups")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();
        auto *ssd = new StandardSettingDialog(mainStack, "storagegroupeditor",
                                              new StorageGroupListEditor());

        if (ssd->Create())
            mainStack->AddScreen(ssd);
        else
            delete ssd;
    }
    else if (sel == "systemeventeditor")
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        auto *msee = new MythSystemEventEditor(mainStack, "System Event Editor");

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
    {
        LOG(VB_GENERAL, LOG_ERR, "Unknown menu action: " + selection);
    }
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
                .arg(badtheme, themename));

    gCoreContext->OverrideSettingForSession("Theme", themename);
    themedir = GetMythUI()->FindThemeDir(themename);

    MythTranslation::reload();
    GetMythMainWindow()->Init();

    return RunMenu(themedir, themename);
}

static int reloadTheme(void)
{
    GetMythUI()->InitThemeHelper();
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
    if (menu)
        menu->Close();
    GetMythMainWindow()->Init();
    GetMythMainWindow()->SetEffectsEnabled(true);

    if (!RunMenu(themedir, themename) && !resetTheme(themedir, themename))
        return GENERIC_EXIT_NO_THEME;

    return 0;
}

int main(int argc, char *argv[])
{
    QString geometry;
    bool    doScan   = false;
    bool    doScanList = false;
    bool    doScanSaveOnly = false;
    bool    scanInteractive = true;
    bool    expertMode = false;
    uint    scanImport = 0;
    bool    scanFTAOnly = false;
    bool    scanLCNOnly = false;
    bool    scanCompleteOnly = false;
    bool    scanFullChannelSearch = false;
    bool    scanRemoveDuplicates = false;
    bool    addFullTS = false;
    ServiceRequirements scanServiceRequirements = kRequireAV;
    uint    scanCardId = 0;
    QString frequencyStandard = "atsc";
    QString modulation = "vsb8";
    QString region = "us";
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
        MythTVSetupCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    bool quiet = false;
    bool use_display = true;
    if (cmdline.toBool("scan"))
    {
        quiet = true;
        use_display = false;
    }

    std::unique_ptr<QCoreApplication> app {nullptr};
    CleanupGuard callCleanup(cleanup);

    if (use_display)
    {
        MythDisplay::ConfigureQtGUI(1, cmdline);
        app = std::make_unique<QApplication>(argc, argv);
    }
    else
    {
        app = std::make_unique<QCoreApplication>(argc, argv);
    }
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHTV_SETUP);

#ifndef _WIN32
    SignalHandler::Init();
#endif

    if (cmdline.toBool("geometry"))
        geometry = cmdline.toString("geometry");

    QString mask("general");
    int retval = cmdline.ConfigureLogging(mask, quiet);
    if (retval != GENERIC_EXIT_OK)
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
    if (cmdline.toBool("fullsearch"))
        scanFullChannelSearch = true;
    if (cmdline.toBool("removeduplicates"))
        scanRemoveDuplicates = true;
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

    if (!geometry.isEmpty())
        MythMainWindow::ParseGeometryOverride(geometry);

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
            std::cerr << "You must enter a valid cardid to scan." << std::endl;
            std::vector<unsigned int> cardids = CardUtil::GetInputIDs();
            if (cardids.empty())
            {
                std::cerr << "But no cards have been defined on this host"
                          << std::endl;
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
            std::cerr << "Valid cards: " << std::endl;
            for (uint id : cardids)
            {
                fprintf(stderr, "%5u: %s %s\n", id,
                        CardUtil::GetRawInputType(id).toLatin1().constData(),
                        CardUtil::GetVideoDevice(id).toLatin1().constData());
            }
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        if (!okInputName)
        {
            std::cerr << "You must enter a valid input to scan this card."
                      << std::endl;
            std::cerr << "Valid input: "
                      << CardUtil::GetInputName(scanCardId).toLatin1().constData()
                      << std::endl;
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

            int scantype { ScanTypeSetting::FullScan_ATSC };
            if (frequencyStandard == "atsc")
                scantype = ScanTypeSetting::FullScan_ATSC; // NOLINT(bugprone-branch-clone)
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
            {
                scantype = ScanTypeSetting::ExternRecImport;
            }
            else
            {
                scantype = ScanTypeSetting::FullScan_ATSC;
            }

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
                         scanFullChannelSearch,
                         scanRemoveDuplicates,
                         addFullTS,
                         scanServiceRequirements,
                         // stuff needed for particular scans
                         /* mplexid   */ 0,
                         startChan, frequencyStandard, modulation, region);
            ret = QCoreApplication::exec();
        }
        return (ret) ? GENERIC_EXIT_NOT_OK : GENERIC_EXIT_OK;
    }

    if (doScanList)
    {
        std::vector<ScanInfo> scans = LoadScanList();

        std::cout<<" scanid cardid sourceid processed        date"<<std::endl;
        for (auto & scan : scans)
        {
            printf("%5i %6i %8i %8s    %20s\n",
                   scan.m_scanid,   scan.m_cardid,
                   scan.m_sourceid, (scan.m_processed) ? "yes" : "no",
                   scan.m_scandate.toString(Qt::ISODate)
                   .toLatin1().constData());
        }
        std::cout<<std::endl;

        return GENERIC_EXIT_OK;
    }

    if (scanImport)
    {
        std::cout<<"*** SCAN IMPORT START ***"<<std::endl;
        {
            ScanDTVTransportList list = LoadScan(scanImport);
            ChannelImporter ci(false, true, true, true, false,
                               scanFTAOnly, scanLCNOnly,
                               scanCompleteOnly,
                               scanFullChannelSearch,
                               scanRemoveDuplicates,
                               scanServiceRequirements);
            ci.Process(list);
        }
        std::cout<<"*** SCAN IMPORT END ***"<<std::endl;
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
    mainWindow->Init();
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

    QCoreApplication::exec();

    ChannelGroup::UpdateChannelGroups();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
