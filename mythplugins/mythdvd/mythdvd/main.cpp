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
#include <qsqldatabase.h>
#include <qmutex.h>
#include <qregexp.h>
#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include <mythtv/mythplugin.h>
#include <mythtv/dialogbox.h>
#include <mythtv/util.h>
#include <mythtv/mythmedia.h>

#include "config.h"
#include "settings.h"
#include "dvdripbox.h"
#include "dbcheck.h"

//
//  Transcode stuff only if we were ./configure'd for it
//
#ifdef TRANSCODE_SUPPORT
void startDVDRipper(void)
{
    DVDRipBox *drb = new DVDRipBox(QSqlDatabase::database(), 
                                   gContext->GetMainWindow(),
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

    if (gContext->GetMainWindow()->HandleMedia( command_string, "DVD://"))
        return;

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

    if (gContext->GetMainWindow()->HandleMedia( command_string, "DVD://"))
        return;

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
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "dvd_settings_play")
    {
        DVDPlayerSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "dvd_settings_rip")
    {
        DVDRipperSettings settings;
        settings.exec(QSqlDatabase::database());
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
        gContext->GetLCDDevice()->switchToTime();
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

void handleMedia(void) 
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
       case 3 : //Rip DVD
           startDVDRipper();
           break;
    }
}

void initKeys(void)
{
    REG_JUMP("Play DVD", "Play a DVD", "", playDVD);
    REG_MEDIA_HANDLER("MythDVD Media Handler", "", "", handleMedia, MEDIATYPE_VIDEO);
#ifdef VCD_SUPPORT
    REG_JUMP("Play VCD", "Play a VCD", "", playVCD);
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
    gsettings.load(QSqlDatabase::database());
    gsettings.save(QSqlDatabase::database());
    DVDPlayerSettings psettings;
    psettings.load(QSqlDatabase::database());
    psettings.save(QSqlDatabase::database());
#ifdef TRANSCODE_SUPPORT
    DVDRipperSettings rsettings;
    rsettings.load(QSqlDatabase::database());
    rsettings.save(QSqlDatabase::database());
#endif

    initKeys();

    return 0;
}

int mythplugin_run(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythdvd_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    runMenu("dvdmenu.xml");

    qApp->removeTranslator(&translator);

    return 0;
}

int mythplugin_config(void)
{
    QTranslator translator( 0 );
    translator.load(PREFIX + QString("/share/mythtv/i18n/mythdvd_") +
                    QString(gContext->GetSetting("Language").lower()) +
                    QString(".qm"), ".");
    qApp->installTranslator(&translator);

    runMenu("dvd_settings.xml");

    qApp->removeTranslator(&translator);

    return 0;
}

