#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>
#include <qsocketnotifier.h>

//#include <cdaudio.h>
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

void startDatabaseTree(MythContext *context, QSqlDatabase *db, 
                       QValueList<Metadata> *playlist)
{
    DatabaseBox dbbox(context, db, playlist);

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
    
    MythContext *context = new MythContext();

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
    if (!context->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    context->LoadQtConfig();

    context->LoadSettingsFiles("mythexplorer-settings.txt");

    if (a.argc() > 1)
        context->SetSetting("StartDir",a.argv()[1]);
    if (a.argc() > 2)
        context->SetSetting("LoadProfile",
                           QString("profile_%1").arg(a.argv()[2]));

    context->SetSetting("Profile",
                        context->GetSetting(context->GetSetting("LoadProfile")));
      
    QString startdir = context->GetSetting("StartDir");
    
    Dirlist md = Dirlist(context, startdir);
    QValueList<Metadata> playlist = md.GetPlaylist();

    startDatabaseTree(context, db, &playlist);

    db->close();

    delete context;

    return 0;
}
