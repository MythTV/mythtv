/*
	main.cpp

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Starting point for the mythDVD module
	Much of mythDVD relies very heavily on the 
	transcode project. More information on transcode
	is available at:
	
	http://www.theorie.physik.uni-goettingen.de/~ostreich/transcode/

*/

#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qmutex.h>
#include <qregexp.h>
#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/dialogbox.h>
#include <mythtv/util.h>
#include <mythtv/mythmedia.h>
#include <mythtv/lcddevice.h>

#include "config.h"
#include "settings.h"
#ifdef TRANSCODE_SUPPORT
#include "dvdripbox.h"
#endif
#include "dbcheck.h"

//
//  Transcode stuff only if we were ./configure'd for it
//
#ifdef TRANSCODE_SUPPORT
void startDVDRipper(void)
{
    DVDRipBox *drb = new DVDRipBox(gContext->GetMainWindow(),
                                   "dvd_rip", "dvd-"); 
    qApp->unlock();
    drb->exec();
    qApp->lock();

    qApp->processEvents();

    delete drb;
}
#endif

//
//  VCD Playing only if we were ./configure's for it.
//
#ifdef VCD_SUPPORT
void playVCD()
{
    //
    //  Get the command string to play a VCD
    //
    QString command_string = gContext->GetSetting("VCDPlayerCommand");

    if(command_string.length() < 1)
    {
        //
        //  User probably never did setup
        //
        DialogBox *no_player_dialog = new DialogBox(gContext->GetMainWindow(),
                    QObject::tr("\n\nYou have no VCD Player command defined."));
        no_player_dialog->AddButton(QObject::tr("OK, I'll go run Setup"));
        no_player_dialog->exec();
        
        delete no_player_dialog;
        return;
    }
    else
    {
        if(command_string.contains("%d"))
        {
            //
            //  Need to do device substitution
            //
            QString vcd_device = gContext->GetSetting("VCDDeviceLocation");
            if(vcd_device.length() < 1)
            {
                //
                //  RTF README
                //
                DialogBox *no_device_dialog = new DialogBox(gContext->GetMainWindow(),
                            QObject::tr("\n\nYou have no VCD Device defined."));
                no_device_dialog->AddButton(QObject::tr("OK, I'll go run Setup"));
                no_device_dialog->exec();
                
                delete no_device_dialog;
                return;
            }
            else
            {
                command_string = command_string.replace( QRegExp("%d"), vcd_device );
            }   
        }
        myth_system(command_string);
        gContext->GetMainWindow()->raise();
        gContext->GetMainWindow()->setActiveWindow();
        gContext->GetMainWindow()->currentWidget()->setFocus();
    }
}
#endif

void playDVD(void)
{
    //
    //  Get the command string to play a DVD
    //
    
    QString command_string = gContext->GetSetting("DVDPlayerCommand");

    if(command_string.length() < 1)
    {
        //
        //  User probably never did setup
        //
        DialogBox *no_player_dialog = new DialogBox(gContext->GetMainWindow(),
                   QObject::tr("\n\nYou have no DVD Player command defined."));
        no_player_dialog->AddButton(QObject::tr("OK, I'll go run Setup"));
        no_player_dialog->exec();
        
        delete no_player_dialog;
        return;
    }
    else
    {
        if(command_string.contains("%d"))
        {
            //
            //  Need to do device substitution
            //
            QString dvd_device = gContext->GetSetting("DVDDeviceLocation");
            if(dvd_device.length() < 1)
            {
                //
                //  RTF README
                //
                DialogBox *no_device_dialog = new DialogBox(gContext->GetMainWindow(),
                           QObject::tr("\n\nYou have no DVD Device defined."));
                no_device_dialog->AddButton(QObject::tr("OK, I'll go run Setup"));
                no_device_dialog->exec();
        
                delete no_device_dialog;
                return;
            }
            else
            {
                command_string = command_string.replace( QRegExp("%d"), dvd_device );
            }
        }
        myth_system(command_string);
        gContext->GetMainWindow()->raise();
        gContext->GetMainWindow()->setActiveWindow();
        gContext->GetMainWindow()->currentWidget()->setFocus();
        
    }
}

void DVDCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.lower();

    if (sel == "dvd_play")
    {
        playDVD();
    }
#ifdef VCD_SUPPORT
    if (sel == "vcd_play")
    {
        playVCD();
    }
#endif
    else if (sel == "dvd_rip")
    {
#ifdef TRANSCODE_SUPPORT
        startDVDRipper();
#else
        cerr << "main.o: TRANSCODE_SUPPORT is not on, but I still got asked to start the DVD ripper" << endl ;
#endif
    }
    else if (sel == "dvd_settings_general")
    {
        DVDGeneralSettings settings;
        settings.exec();
    }
    else if (sel == "dvd_settings_play")
    {
        DVDPlayerSettings settings;
        settings.exec();
    }
    else if (sel == "dvd_settings_rip")
    {
        DVDRipperSettings settings;
        settings.exec();
    }
}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();

    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu, 
                                      gContext->GetMainWindow(), "dvd menu");

    diag->setCallback(DVDCallback, NULL);
    diag->setKillable();

    if (diag->foundTheme())
    {
        if (class LCD * lcd = LCD::Get())
        {
            lcd->switchToTime();
        }
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    delete diag;
}

extern "C" {
int mythplugin_init(const char *libversion);
int mythplugin_run(void);
int mythplugin_config(void);
}

void handleDVDMedia(MythMediaDevice *) 
{
    switch (gContext->GetNumSetting("DVDOnInsertDVD", 1))
    {
        case 0 : // Do nothing
            break;
        case 1 : // Display menu (mythdvd)*/
            mythplugin_run();
            break;
        case 2 : // play DVD
            playDVD();    
            break;
#ifdef TRANSCODE_SUPPORT
        case 3 : //Rip DVD
            startDVDRipper();
            break;
#endif
        default:
            cerr << "mythdvd main.o: handleMedia() does not know what to do"
                 << endl;
    }
}

#ifdef VCD_SUPPORT
void handleVCDMedia(MythMediaDevice *) 
{
    switch (gContext->GetNumSetting("DVDOnInsertDVD", 1))
    {
       case 0 : // Do nothing
           break;
       case 1 : // Display menu (mythdvd)*/
           mythplugin_run();
           break;
       case 2 : // play VCD
           playVCD();
           break;
       case 3 : // Do nothing, cannot rip VCD?
           break;
    }
}
#endif

void initKeys(void)
{
    REG_JUMP("Play DVD", "Play a DVD", "", playDVD);
    REG_MEDIA_HANDLER("MythDVD DVD Media Handler", "", "", handleDVDMedia, MEDIATYPE_DVD);
#ifdef VCD_SUPPORT
    REG_JUMP("Play VCD", "Play a VCD", "", playVCD);
    REG_MEDIA_HANDLER("MythDVD VCD Media Handler", "", "", handleVCDMedia, MEDIATYPE_VCD);
#endif
#ifdef TRANSCODE_SUPPORT
    REG_JUMP("Rip DVD", "Import a DVD into your MythVideo database", "", 
             startDVDRipper);
#endif
}

int mythplugin_init(const char *libversion)
{
    if (!gContext->TestPopupVersion("mythdvd", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    UpgradeDVDDatabaseSchema();

    DVDGeneralSettings gsettings;
    gsettings.load();
    gsettings.save();
    DVDPlayerSettings psettings;
    psettings.load();
    psettings.save();
#ifdef TRANSCODE_SUPPORT
    DVDRipperSettings rsettings;
    rsettings.load();
    rsettings.save();
#endif

    initKeys();

    return 0;
}

int mythplugin_run(void)
{
    runMenu("dvdmenu.xml");

    return 0;
}

int mythplugin_config(void)
{
    runMenu("dvd_settings.xml");

    return 0;
}

