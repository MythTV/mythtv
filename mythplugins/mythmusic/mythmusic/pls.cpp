/*
  playlistfile (.pls) parser
  Eskil Heyn Olsen, 2005, distributed under the GPL as part of mythtv.

  Update July 2010 updated for Qt4 (Paul Harrison)
  Update December 2012 updated to use QSettings for the pls parser
  Update September 2014 add simple asx parser
*/

// c++
#include <string>

// qt
#include <QPair>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QFileInfo>
#include <QSettings>
#include <QDomDocument>

// mythtv
#include <mythlogging.h>

// mythmusic
#include "pls.h"


using namespace std;


PlayListFile::~PlayListFile(void)
{
    clear();
}

int PlayListFile::parse(PlayListFile *pls, const QString &filename)
{
    int result = 0;
    QString extension = QFileInfo(filename).suffix().toLower();

    if (extension == "pls")
        result = PlayListFile::parsePLS(pls, filename);
    else if (extension == "m3u")
        result = PlayListFile::parseM3U(pls, filename);
    else if (extension == "asx")
        result = PlayListFile::parseASX(pls, filename);

    return result;
}

int PlayListFile::parsePLS(PlayListFile *pls, const QString &filename)
{
    LOG(VB_FILE, LOG_DEBUG, QString("DecoderHandler: parsePLS - '%1'").arg(filename));

    QSettings settings(filename, QSettings::IniFormat);

    // we allow both 'playlist' and 'Playlist' for the group name
    QStringList groups = settings.childGroups();

    if (groups.contains("playlist"))
        settings.beginGroup("playlist");
    else if (groups.contains("Playlist"))
        settings.beginGroup("Playlist");
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("DecoderHandler: parsePLS - playlist group not found"));
        return 0;
    }

    int num_entries = -1;

    // Some pls files have "numberofentries", some have "NumberOfEntries".
    QStringList keys = settings.childKeys();

    if (keys.contains("numberofentries"))
        num_entries = settings.value("numberofentries", -1).toInt();
    else if (keys.contains("NumberOfEntries"))
        num_entries = settings.value("NumberOfEntries", -1).toInt();
    else
    {
        LOG(VB_GENERAL, LOG_ERR, QString("DecoderHandler: parsePLS - NumberOfEntries key not found"));
        return 0;
    }

    for (int n = 1; n <= num_entries; n++)
    {
        auto *e = new PlayListFileEntry();
        QString t_key = QString("Title%1").arg(n);
        QString f_key = QString("File%1").arg(n);
        QString l_key = QString("Length%1").arg(n);

        e->setFile(settings.value(f_key).toString());
        e->setTitle(settings.value(t_key).toString());
        e->setLength(settings.value(l_key).toInt());

        pls->add(e);
    }

    return pls->size();
}

#define M3U_HEADER  "#EXTM3U"
#define M3U_INFO    "#EXTINF"

int PlayListFile::parseM3U(PlayListFile *pls, const QString &filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly))
        return 0;

    QTextStream stream(&f);
    QString data = stream.readAll();
    QStringList lines = data.split(QRegExp("[\r\n]"));

    QStringList::iterator it;
    for (it = lines.begin(); it != lines.end(); ++it)
    {
        // ignore empty lines
        if (it->isEmpty())
            continue;

        // ignore the M3U header
        if (it->startsWith(M3U_HEADER))
            continue;

        // for now ignore M3U info lines
        if (it->startsWith(M3U_INFO))
            continue;

        // add to the playlist
        auto *e = new PlayListFileEntry();
        e->setFile(*it);
        e->setTitle(*it);
        e->setLength(-1);

        pls->add(e);
    }

    return pls->size();
}

int PlayListFile::parseASX(PlayListFile *pls, const QString &filename)
{
    QDomDocument doc("mydocument");
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return 0;

    if (!doc.setContent(&file))
    {
        file.close();
        return 0;
    }
    file.close();

    //QDomElement docElem = doc.documentElement();
    QDomNodeList entryList = doc.elementsByTagName("Entry");
    QString url;

    for (int x = 0; x < entryList.count(); x++)
    {
        QDomNode n = entryList.item(x);
        QDomElement elem = n.toElement();
        QDomNodeList refList = elem.elementsByTagName("ref");
        for (int y = 0; y < refList.count(); y++)
        {
            QDomNode n2 = refList.item(y);
            QDomElement elem2 = n2.toElement();
            if (!elem2.isNull())
            {
                url = elem2.attribute("href");

                // add to the playlist
                auto *e = new PlayListFileEntry();
                e->setFile(url.replace("mms://", "mmsh://"));
                e->setTitle(url.replace("mms://", "mmsh://"));
                e->setLength(-1);

                pls->add(e);
            }
        }
    }

    return pls->size();
}
