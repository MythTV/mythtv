/*
  playlistfile (.pls) parser
  Eskil Heyn Olsen, 2005, distributed under the GPL as part of mythtv.

  Update July 2010 updated for Qt4 (Paul Harrison)
  Update December 2012 updated to use QSettings for the pls parser
*/

// c
//#include "iostream"
#include <string>

// qt
#include <QPair>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QFileInfo>
#include <QSettings>

// mythtv
#include <mythlogging.h>

// mythmusic
#include "pls.h"


using namespace std;


PlayListFile::PlayListFile(void) : m_version(0)
{
}

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

    return result;
}

int PlayListFile::parsePLS(PlayListFile *pls, const QString &filename)
{
    QSettings settings(filename, QSettings::IniFormat);
    settings.beginGroup("playlist");

    int num_entries = settings.value("numberofentries", -1).toInt();

    // Some pls files have "numberofentries", some has "NumberOfEntries".
    if (num_entries == -1)
        num_entries = settings.value("NumberOfEntries", -1).toInt();

    for (int n = 1; n <= num_entries; n++)
    {
        PlayListFileEntry *e = new PlayListFileEntry();
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
        PlayListFileEntry *e = new PlayListFileEntry();
        e->setFile(*it);
        e->setTitle(*it);
        e->setLength(-1);

        pls->add(e);
    }

    return pls->size();
}
