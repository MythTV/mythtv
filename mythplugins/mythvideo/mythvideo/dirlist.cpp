#include <qdir.h>
#include <iostream>
using namespace std;

#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "metadata.h"
#include "databasebox.h"
#include "dirlist.h"

#include <mythtv/themedmenu.h>
#include <mythtv/mythcontext.h>

Metadata * Dirlist::CheckFile(QString &filename)
{
    QString s = filename.section( '/',-1); // s == "myapp"

    Metadata *retdata = new Metadata(filename, s, "album", "title", "genre",
                                     1900, 3, 40);
    return(retdata);
}

Dirlist::Dirlist(QString &directory)
{
    QDir d(directory);
    d.setSorting(QDir::DirsFirst| QDir::Name | QDir::IgnoreCase);
    Metadata *data;
    if (!d.exists())
      ;
    const QFileInfoList *list = d.entryInfoList();
    if (!list)
      ;

    QFileInfoListIterator it(*list);
    QFileInfo *fi;

    while ((fi = it.current()) != 0)
    {
        if (fi->fileName() == ".")
        {
            ++it;
	    continue;
        }

        if (fi->fileName() == "..")
        {
	    Metadata retdata(fi->absFilePath(), fi->absFilePath(), "album", 
                             "title", "dir",1900, 3, 40);
            playlist.append(retdata);
	    ++it;
            continue;
        }
	
        QString filename = fi->absFilePath();
        if (fi->isDir())
        {
            QString s = filename.section( '/',-1);
	    Metadata retdata(filename, s, "album", "title", "dir",1900, 3, 40);
            playlist.append(retdata);
        }
        else
        {
            QString ext = filename.section('.',-1);
	    //printf("profile:%s\n",gContext->GetSetting("Profile").ascii());
	    QString prof = gContext->GetSetting("Profile");
	    QString prof_name = "profile_" + prof;
	    if (gContext->GetSetting(prof_name).contains(ext))
            {
                data = CheckFile(filename);
                playlist.append(*data);
                delete data;
	    }
	}

        ++it;
    }
}

