#include "playlist.h"

void LoadDefaultPlaylist(QSqlDatabase *db, QValueList<Metadata> &playlist)
{
    QString thequery = "SELECT songlist FROM musicplaylist WHERE "
                       "name = \"default_playlist_storage\";";

    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        query.next();

        QString songlist = query.value(0).toString();

        QStringList list = QStringList::split(",", songlist);

        QStringList::iterator it = list.begin();
        for (; it != list.end(); it++)
        {
            unsigned int id = QString(*it).toUInt();

            Metadata mdata;

            mdata.setID(id);

            mdata.fillDataFromID(db);

            if (mdata.Filename() != "")
                playlist.push_back(mdata);
        }
    }
}

void SaveDefaultPlaylist(QSqlDatabase *db, QValueList<Metadata> &playlist)
{
    QString playliststring;

    QValueList<Metadata>::iterator it = playlist.begin();

    bool first = true;
    for (; it != playlist.end(); it++)
    {
        unsigned int id = (*it).ID();

        if (!first)
            playliststring += ",";
        playliststring += QString("%1").arg(id);
        first = false;
    }

    QString thequery = "SELECT NULL FROM musicplaylist WHERE name = "
                       "\"default_playlist_storage\";";
    QSqlQuery query = db->exec(thequery);

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        thequery = QString("UPDATE musicplaylist SET songlist = \"%1\" WHERE "
                           "name = \"default_playlist_storage\";")
                           .arg(playliststring);
    }
    else
    {
        thequery = QString("INSERT musicplaylist (name,songlist) "
                           "VALUES(\"default_playlist_storage\",\"%1\");")
                           .arg(playliststring);
    }
    query = db->exec(thequery);
}
