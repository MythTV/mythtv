#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>
#include <qsocketnotifier.h>
#include <qtextcodec.h>

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

MythContext *gContext;

void startDatabaseTree(QSqlDatabase *db, QValueList<Metadata> *playlist)
{
    DatabaseBox dbbox(db, playlist);
    dbbox.Show();
    dbbox.exec();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTranslator translator( 0 );
    
    gContext = new MythContext(MYTH_BINARY_VERSION);

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

    translator.load(PREFIX + QString("/share/mythtv/i18n/mythvideo_") + 
                    QString(gContext->GetSetting("Language").lower()) + 
                    QString(".qm"), ".");
    a.installTranslator(&translator);

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
