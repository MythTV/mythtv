#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

//#include <cdaudio.h>

#include "metadata.h"

#include "databasebox.h"



#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>


Metadata * CheckFile(MythContext *context, QString &filename)
{
  QString s = filename.section( '/',-1); // s == "myapp"

    Metadata *retdata = new Metadata(filename, s, "album", "title", "genre",
                                     1900, 3, 40);
    return(retdata);
}

void SearchDir(MythContext *context, QString &directory,QValueList<Metadata> &playlist)
{
    QDir d(directory);
    Metadata *data;
    if (!d.exists())
        return;
    const QFileInfoList *list = d.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        ++it;
        if (fi->fileName() == "." || fi->fileName() == "..")
            continue;
        QString filename = fi->absFilePath();
        if (fi->isDir())
            SearchDir(context, filename,playlist);
        else
	  {
	    QString ext=filename.section('.',-1);
	    if(context->GetSetting("Profile").contains(ext))
	      {
		data=CheckFile(context, filename);
	    	    playlist.append(*data);
	      }
	  }
    }
}


void startDatabaseTree(MythContext *context, QSqlDatabase *db, QString &paths, 
                       QValueList<Metadata> *playlist)
{
    DatabaseBox dbbox(context, db, paths, playlist);
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

    QValueList<Metadata> playlist;

    if (startdir != "")
        SearchDir(context, startdir,playlist);


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
