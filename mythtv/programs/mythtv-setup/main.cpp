#include <iostream>
#include <memory>

#include <QString>
#include <QDir>
#include <QMap>
#include <QApplication>

#include "mythconfig.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "langsettings.h"
#include "exitcodes.h"
#include "exitprompt.h"
#include "storagegroupeditor.h"
#include "myththemedmenu.h"
#include "myththemebase.h"
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

using namespace std;

static MythThemeBase *themeBase   = NULL;
ExitPrompter   *exitPrompt  = NULL;
StartPrompter  *startPrompt = NULL;

void SetupMenuCallback(void* data, QString& selection)
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
    else if (sel == "exiting_app")
    {
        if (!exitPrompt)
            exitPrompt = new ExitPrompter();
        exitPrompt->handleExit();
    }
    else
        VERBOSE(VB_IMPORTANT, "Unknown menu action: " + selection);
}

bool SetupMenu(QString themedir, QString themename)
{
    QByteArray tmp = themedir.toLocal8Bit();
    MythThemedMenu *menu = new MythThemedMenu(
        QString(tmp.constData()), "setup.xml",
        GetMythMainWindow()->GetMainStack(), "mainmenu", false);

    if (menu->foundTheme())
    {
        menu->setCallback(SetupMenuCallback, gContext);
        GetMythMainWindow()->GetMainStack()->AddScreen(menu);
        return true;
    }

    VERBOSE(VB_IMPORTANT, QString("Couldn't use theme '%1'").arg(themename));
    delete menu;
    menu = NULL;

    return false;
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
bool resetTheme(QString themedir, const QString badtheme)
{
    QString themename = DEFAULT_UI_THEME;

    if (badtheme == DEFAULT_UI_THEME)
        themename = FALLBACK_UI_THEME;

    VERBOSE(VB_IMPORTANT,
                QString("Overriding broken theme '%1' with '%2'")
                .arg(badtheme).arg(themename));

    gCoreContext->OverrideSettingForSession("Theme", themename);
    themedir = GetMythUI()->FindThemeDir(themename);

    LanguageSettings::reload();
    GetMythUI()->LoadQtConfig();
    GetMythMainWindow()->Init();
    themeBase->Reload();
    GetMythUI()->UpdateImageCache();

    GetMythMainWindow()->ReinitDone();

    return SetupMenu(themedir, themename);
}

static void print_usage()
{
        cerr << "Valid options are: "<<endl
#ifdef USING_X11
             << "-display X-server              "
            "Create GUI on X-server, not localhost" << endl
#endif
             << "-geometry or --geometry WxH    "
                "Override window size settings" << endl
             << "-geometry WxH+X+Y              "
                "Override window size and position" << endl
             << "-O or " << endl
             << "  --override-setting KEY=VALUE "
                "Force the setting named 'KEY' to value 'VALUE'" << endl
             << "-v or --verbose debug-level    "
                "Use '-v help' for level info" << endl
             << endl;
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
    bool    use_display = true;

    for(int argpos = 1; argpos < argc; ++argpos)
    {
        if (!strcmp(argv[argpos], "-h") ||
            !strcmp(argv[argpos], "--help") ||
            !strcmp(argv[argpos], "--usage"))
        {
            print_usage();
            return GENERIC_EXIT_OK;
        }
#ifdef USING_X11
    // Remember any -display or -geometry argument
    // which QApplication init will remove.
        else if (argpos+1 < argc && !strcmp(argv[argpos],"-geometry"))
            geometry = argv[argpos+1];
        else if (argpos+1 < argc && !strcmp(argv[argpos],"-display"))
            display = argv[argpos+1];
#endif
        else if (QString(argv[argpos]).left(6) == "--scan")
        {
            use_display = false;
            print_verbose_messages = VB_NONE;
            verboseString = "";
        }
    }

#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(FALSE);
#endif
    QApplication a(argc, argv, use_display);

    QMap<QString, QString> settingsOverride;

    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-display") ||
            !strcmp(a.argv()[argpos],"--display"))
        {
            if (a.argc()-1 > argpos)
            {
                display = a.argv()[argpos+1];
                if (display.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -display option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                }
                else
                    ++argpos;
            }
            else
            {
                cerr << "Missing argument to -display option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-geometry") ||
                 !strcmp(a.argv()[argpos],"--geometry"))
        {
            if (a.argc()-1 > argpos)
            {
                geometry = a.argv()[argpos+1];
                if (geometry.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to "
                        "-geometry option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                }
                else
                    ++argpos;
            }
            else
            {
                cerr << "Missing argument to -geometry option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-v") ||
                  !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return BACKEND_EXIT_INVALID_CMDLINE;
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-O") ||
                 !strcmp(a.argv()[argpos],"--override-setting"))
        {
            if (a.argc()-1 > argpos)
            {
                QString tmpArg = a.argv()[argpos+1];
                if (tmpArg.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to "
                            "-O/--override-setting option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                }

                QStringList pairs = tmpArg.split(",");
                for (int index = 0; index < pairs.size(); ++index)
                {
                    QStringList tokens = pairs[index].split("=");
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    settingsOverride[tokens[0]] = tokens[1];
                }
            }
            else
            {
                cerr << "Invalid or missing argument to "
                        "-O/--override-setting option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }

            ++argpos;
        }
        else if (!strcmp(a.argv()[argpos],"--expert"))
        {
            expertMode = true;
        }
        else if (!strcmp(a.argv()[argpos],"--scan-list"))
        {
            doScanList = true;
        }
        else if (!strcmp(a.argv()[argpos],"--scan-import"))
        {
            if (a.argc()-1 > argpos)
            {
                QString tmpArg = a.argv()[argpos + 1];
                scanImport = tmpArg.toUInt();
                ++argpos;

                if (a.argc()-1 > argpos)
                {
                    tmpArg = a.argv()[argpos + 1];
                    scanFTAOnly = tmpArg.toUInt();
                    ++argpos;
                }

                if (a.argc()-1 > argpos)
                {
                    tmpArg = a.argv()[argpos + 1];
                    tmpArg = tmpArg.toLower();
                    scanServiceRequirements = kRequireNothing;
                    if (tmpArg.contains("radio"))
                        scanServiceRequirements = kRequireAudio;
                    if (tmpArg.contains("tv"))
                        scanServiceRequirements = kRequireAV;
                    if (tmpArg.contains("tv+radio") ||
                        tmpArg.contains("radio+tv"))
                        scanServiceRequirements = kRequireAudio;
                    if (tmpArg.contains("all"))
                        scanServiceRequirements = kRequireNothing;
                    ++argpos;
                }
            }
            else
            {
                cerr << "Missing scan number for import, please run "
                     << "--scan-list to list importable scans." << endl;
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--scan-save-only"))
        {
            doScanSaveOnly = true;
        }
        else if (!strcmp(a.argv()[argpos],"--scan-non-interactive"))
        {
            scanInteractive = false;
        }
        else if (!strcmp(a.argv()[argpos],"--scan"))
        {
            if (a.argc()-1 > argpos)
            {
                scanTableName = a.argv()[argpos + 1];
                ++argpos;
            }
            if (a.argc()-1 > argpos)
            {
                QString tmpArg = a.argv()[argpos + 1];
                scanCardId = tmpArg.toUInt();
                ++argpos;
            }
            if (a.argc()-1 > argpos)
            {
                scanInputName = a.argv()[argpos + 1];
                ++argpos;
            }
            doScan = true;
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            print_usage();
            return BACKEND_EXIT_INVALID_CMDLINE;
        }
    }

    if (!display.isEmpty())
    {
        MythUIHelper::SetX11Display(display);
    }

    if (!geometry.isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(geometry);
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);

    std::auto_ptr<MythContext> contextScopeDelete(gContext);

    if (!gContext->Init(use_display))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

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

    if (!MSqlQuery::testDBConnection())
    {
        cerr << "Unable to open database.\n";
        //     << "Driver error was:" << endl
        //     << db->lastError().driverText() << endl
        //     << "Database error was:" << endl
        //     << db->lastError().databaseText() << endl;

        return GENERIC_EXIT_DB_ERROR;
    }

    if (use_display)
    {
        gCoreContext->SetSetting("Theme", DEFAULT_UI_THEME);
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
            scanInputName = CardUtil::GetDefaultInput(scanCardId);

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
                return BACKEND_EXIT_INVALID_CMDLINE;
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
            return BACKEND_EXIT_INVALID_CMDLINE;
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
            return BACKEND_EXIT_INVALID_CMDLINE;
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
            return BACKEND_EXIT_INVALID_CMDLINE;
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
            ret = a.exec();
        }
        return (ret) ? GENERIC_EXIT_NOT_OK : BACKEND_EXIT_OK;
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
                   scans[i].scandate.toString().toAscii().constData());
        }
        cout<<endl;

        return BACKEND_EXIT_OK;
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
        return BACKEND_EXIT_OK;
    }

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    mainWindow->setWindowTitle(QObject::tr("MythTV Setup"));

    themeBase = new MythThemeBase();
    GetMythUI()->UpdateImageCache();
    (void) themeBase;

    LanguageSettings::prompt();
    LanguageSettings::load("mythfrontend");

    if (!UpgradeTVDatabaseSchema(true))
    {
        VERBOSE(VB_IMPORTANT, "Couldn't upgrade database to new schema.");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    // Refresh Global/Main Menu keys after DB update in case there was no DB
    // when they were written originally
    mainWindow->ResetKeys();

    if (!startPrompt)
        startPrompt = new StartPrompter();
    startPrompt->handleStart();

    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't find theme '%1'")
                .arg(themename));
        return FRONTEND_BUGGY_EXIT_NO_THEME;
    }

    // Let the user select buttons, type values, scan for channels, etc.
    if (!SetupMenu(themedir, themename) && !resetTheme(themedir, themename))
        return FRONTEND_BUGGY_EXIT_NO_THEME;

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
            VERBOSE(VB_IMPORTANT, "Unable to create expert settings editor "
                    "window");
            return BACKEND_EXIT_OK;
        }
    }

    qApp->exec();
    // Main menu callback to ExitPrompter does CheckSetup(), cleanup and exit.
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
