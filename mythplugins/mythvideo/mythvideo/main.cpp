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
#include "lirc_client.h"
int lfd;
    struct lirc_config *config;

void startDatabaseTree(MythContext *context, QSqlDatabase *db, QString &paths, 
                       QValueList<Metadata> *playlist)
{
  int flags;
    DatabaseBox dbbox(context, db, paths, playlist);
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


    dbbox.Show();

    dbbox.exec();
}


struct MusicData
{
    MythContext *context;
    QString paths;
    QSqlDatabase *db;
    QString startdir;

    QValueList<Metadata> *playlist;
};

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    
    MythContext *context = new MythContext();

    lfd=lirc_init("mythvideo",1);
    lirc_readconfig(NULL,&config,NULL);

    context->LoadQtConfig();

    context->LoadSettingsFiles("mythexplorer-settings.txt");
    if(a.argc() > 1)
      context->SetSetting("StartDir",a.argv()[1]);
    if(a.argc() > 2)
      context->SetSetting("LoadProfile",QString("profile_%1").arg(a.argv()[2]));
    context->SetSetting("Profile",context->GetSetting(context->GetSetting("LoadProfile")));
      
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

    QString startdir = context->GetSetting("StartDir");
    
    //    if (startdir != "")
        Dirlist md = Dirlist(context, startdir);
 
   QValueList<Metadata> playlist = md.GetPlaylist();


    QString themename = context->GetSetting("Theme");
    QString themedir = context->FindThemeDir(themename);

    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }
 
    MusicData data;
   
    data.context = context;
    data.db = db;
    data.startdir = startdir;
    data.playlist = &playlist;
       startDatabaseTree(data.context, data.db, data.paths, 
                          data.playlist);


    db->close();

    delete context;

    return 0;
}
