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

#include "config.h"
#include "settings.h"
#include "dvdripbox.h"

//
//  Transcode stuff only if we were ./configure'd for it
//
#ifdef TRANSCODE_SUPPORT
void startDVDRipper(QSqlDatabase *db)
{
    DVDRipBox *drb = new DVDRipBox(db, gContext->GetMainWindow(),
                                   "dvd_rip", "dvd-"); 
    qApp->unlock();
    drb->exec();
    qApp->lock();

    qApp->processEvents();

    delete drb;
}
#endif

void playDVD()
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
                     "\n\nYou have no DVD Player command defined.");
        no_player_dialog->AddButton("OK, I'll go run Setup");
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
                             "\n\nYou have no DVD Device defined.");
                no_device_dialog->AddButton("OK, I'll go run Setup");
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

struct DVDData
{
    QSqlDatabase *db;
};

void DVDCallback(void *data, QString &selection)
{
    DVDData *ddata = (DVDData *)data;
    QString sel = selection.lower();

    if (sel == "dvd_play")
    {
        playDVD();
    }
    else if (sel == "dvd_rip")
    {
#ifdef TRANSCODE_SUPPORT
        startDVDRipper(ddata->db);
#else
        ddata = ddata; // -Wall
        cerr << "main.o: TRANSCODE_SUPPORT is not on, but I still got asked to start the DVD ripper" << endl ;
#endif
    }
    else if (sel == "dvd_settings_general")
    {
        GeneralSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "dvd_settings_play")
    {
        PlayerSettings settings;
        settings.exec(QSqlDatabase::database());
    }
    else if (sel == "dvd_settings_rip")
    {
        RipperSettings settings;
        settings.exec(QSqlDatabase::database());
    }
}

void runMenu(QString which_menu)
{
    QString themedir = gContext->GetThemeDir();
    QSqlDatabase *db = QSqlDatabase::database();

    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), which_menu, 
                                      gContext->GetMainWindow(), "dvd menu");

    DVDData data;
    data.db = db;

    diag->setCallback(DVDCallback, &data);
    diag->setKillable();

    if (diag->foundTheme())
    {
        gContext->LCDswitchToTime();
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

int mythplugin_init(const char *libversion)
{

    if (!gContext->TestPopupVersion("mythdvd", libversion,
                                    MYTH_BINARY_VERSION))
        return -1;

    GeneralSettings gsettings;
    gsettings.load(QSqlDatabase::database());
    gsettings.save(QSqlDatabase::database());
    PlayerSettings psettings;
    psettings.load(QSqlDatabase::database());
    psettings.save(QSqlDatabase::database());
#ifdef TRANSCODE_SUPPORT
    RipperSettings rsettings;
    rsettings.load(QSqlDatabase::database());
    rsettings.save(QSqlDatabase::database());
#endif

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

