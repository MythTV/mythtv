#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>
#include <qsocketnotifier.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>


#include "metadata.h"
#include "databasebox.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>
#include "dirlist.h"

#ifdef ENABLE_LIRC
#include "lirc_client.h"

int lfd;
struct lirc_config *config;
#endif

MythContext *gContext;

void startDatabaseTree(QSqlDatabase *db, QValueList<Metadata> *playlist)
{
    DatabaseBox dbbox(db, playlist);

#ifdef ENABLE_LIRC
    int flags;
    QSocketNotifier *sn;

    fcntl(lfd,F_SETOWN,getpid());
    flags=fcntl(lfd,F_GETFL,0);
    if(flags!=-1)
    {
	fcntl(lfd,F_SETFL,flags|O_NONBLOCK);
    }
 
    sn = new QSocketNotifier( lfd, QSocketNotifier::Read, NULL);
    QObject::connect( sn, SIGNAL(activated(int)),
		      &dbbox, SLOT(dataReceived()) );

#endif

    dbbox.Show();
    dbbox.exec();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    gContext = new MythContext();

#ifdef ENABLE_LIRC
    lfd=lirc_init("mythvideo",1);
    lirc_readconfig(NULL,&config,NULL);
#endif

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    gContext->LoadQtConfig();

    gContext->LoadSettingsFiles("mythvideo-settings.txt");

    if (a.argc() > 1)
        gContext->SetSetting("StartDir",a.argv()[1]);
    if (a.argc() > 2)
        gContext->SetSetting("LoadProfile",
                             QString("profile_%1").arg(a.argv()[2]));

    gContext->SetSetting("Profile",
                    gContext->GetSetting(gContext->GetSetting("LoadProfile")));
      
    QString startdir = gContext->GetSetting("StartDir");
    
    Dirlist md = Dirlist(startdir);
    QValueList<Metadata> playlist = md.GetPlaylist();

    startDatabaseTree(db, &playlist);

    db->close();

    delete gContext;

    return 0;
}
