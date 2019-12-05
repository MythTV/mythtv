#include <iostream>

#include "musicmetadata.h"

// qt
#include <QRegExp>
#include <QDomDocument>

// mythtv
#include "mythcontext.h"

// libmythmetadata
#include "lyricsdata.h"


/*************************************************************************/
//LyricsData

//LyricsData::LyricsData():
//    m_parent(nullptr), m_status(STATUS_NOTLOADED), m_syncronized(false), m_changed(false) { }

LyricsData::~LyricsData()
{
    clear();
}

void LyricsData::clear(void)
{
    m_grabber = m_artist = m_album = m_title = "";

    clearLyrics();

    m_syncronized = false;
    m_changed = false;
    m_status = STATUS_NOTLOADED;
}

void LyricsData::clearLyrics(void)
{
    QMap<int, LyricsLine*>::iterator i = m_lyricsMap.begin();
    while (i != m_lyricsMap.end()) 
    {
        delete i.value();
        ++i;
    }

    m_lyricsMap.clear();
}

void LyricsData::findLyrics(const QString &grabber)
{
    if (!m_parent)
        return;

    switch (m_status)
    {
        case STATUS_SEARCHING:
        {
            emit statusChanged(m_status, tr("Searching..."));
            return;
        }

        case STATUS_FOUND:
        {
            emit statusChanged(m_status, "");
            return;
        }

        case STATUS_NOTFOUND:
        {
            emit statusChanged(m_status, tr("No lyrics found for this track"));
            return;
        }

        default:
            break;
    }

    clear();

    m_status = STATUS_SEARCHING;

    // don't bother searching if we have no title, artist and album
    if (m_parent->Title().isEmpty() && m_parent->Artist().isEmpty() && m_parent->Album().isEmpty())
    {
        m_status = STATUS_NOTFOUND;
        emit statusChanged(m_status, tr("No lyrics found for this track"));
        return;
    }

    // listen for messages
    gCoreContext->addListener(this);

    // send a message to the master BE to find the lyrics for this track
    QStringList slist;
    slist << "MUSIC_LYRICS_FIND"
          << m_parent->Hostname()
          << QString::number(m_parent->ID())
          << grabber;

    QString title = m_parent->Title().isEmpty() ? "*Unknown*" : m_parent->Title();
    QString artist = m_parent->Artist().isEmpty() ? "*Unknown*" : m_parent->Artist();
    QString album = m_parent->Album().isEmpty() ? "*Unknown*" : m_parent->Album();

    if (!m_parent->isDBTrack())
        slist << artist
              << album
              << title;

   LOG(VB_NETWORK, LOG_INFO, QString("LyricsData:: Sending command %1").arg(slist.join('~')));

   gCoreContext->SendReceiveStringList(slist);
}

void LyricsData::save(void)
{
    // only save the lyrics if they have been changed
    if (!m_changed)
        return;

    // only save lyrics if it is a DB track
    if (!m_parent || !m_parent->isDBTrack())
        return;

    // send a message to the master BE to save the lyrics for this track
    QStringList slist;
    slist << "MUSIC_LYRICS_SAVE"
          << m_parent->Hostname()
          << QString::number(m_parent->ID());

    slist << createLyricsXML();

    gCoreContext->SendReceiveStringList(slist);
}

QString LyricsData::createLyricsXML(void)
{
    QDomDocument doc("lyrics");

    QDomElement root = doc.createElement("lyrics");
    doc.appendChild(root);

    // artist
    QDomElement artist = doc.createElement("artist");
    root.appendChild(artist);
    artist.appendChild(doc.createTextNode(m_artist));

    // album
    QDomElement album = doc.createElement("album");
    root.appendChild(album);
    album.appendChild(doc.createTextNode(m_album));

    // title
    QDomElement title = doc.createElement("title");
    root.appendChild(title);
    title.appendChild(doc.createTextNode(m_title));

    // syncronized
    QDomElement syncronized = doc.createElement("syncronized");
    root.appendChild(syncronized);
    syncronized.appendChild(doc.createTextNode(m_syncronized ? "True" : "False"));

    // grabber
    QDomElement grabber = doc.createElement("grabber");
    root.appendChild(grabber);
    grabber.appendChild(doc.createTextNode(m_grabber));

    // lyrics
    QMap<int, LyricsLine*>::iterator i = m_lyricsMap.begin();
    while (i != m_lyricsMap.end())
    {
        LyricsLine *line = (*i);
        QDomElement lyric = doc.createElement("lyric");
        root.appendChild(lyric);
        lyric.appendChild(doc.createTextNode(line->toString(m_syncronized)));
        ++i;
    }

    return doc.toString(4);
}

void LyricsData::customEvent(QEvent *event)
{
    if (event->type() == MythEvent::MythEventMessage)
    {
        auto *me = dynamic_cast<MythEvent*>(event);
        if (!me)
            return;

        // we are only interested in MUSIC_LYRICS_* messages
        if (me->Message().startsWith("MUSIC_LYRICS_"))
        {
            QStringList list = me->Message().simplified().split(' ');

            if (list.size() >= 2)
            {
                uint songID = list[1].toUInt();

                // make sure the message is for us
                if (m_parent->ID() == songID)
                {
                    if (list[0] == "MUSIC_LYRICS_FOUND")
                    {
                        gCoreContext->removeListener(this);

                        QString xmlData = me->Message().section(" ", 2, -1);

                        // we found some lyrics so load them
                        loadLyrics(xmlData);
                        emit statusChanged(m_status, "");
                    }
                    else if (list[0] == "MUSIC_LYRICS_STATUS")
                    {
                        emit statusChanged(STATUS_SEARCHING, me->Message().section(" ", 2, -1));
                    }
                    else
                    {
                        gCoreContext->removeListener(this);
                        // nothing found or an error occured
                        m_status = STATUS_NOTFOUND;
                        emit statusChanged(m_status, tr("No lyrics found for this track"));
                    }
                }
            }
        }
    }
}

void LyricsData::loadLyrics(const QString &xmlData)
{
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;

    if (!domDoc.setContent(xmlData, false, &errorMsg, &errorLine, &errorColumn))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("LyricsData:: Could not parse lyrics from %1").arg(xmlData) +
            QString("\n\t\t\tError at line: %1  column: %2 msg: %3").arg(errorLine).arg(errorColumn).arg(errorMsg));
        m_status = STATUS_NOTFOUND;
        return;
    }

    QDomNodeList itemList = domDoc.elementsByTagName("lyrics");
    QDomNode itemNode = itemList.item(0);

    m_grabber = itemNode.namedItem(QString("grabber")).toElement().text();
    m_artist = itemNode.namedItem(QString("artist")).toElement().text();
    m_album = itemNode.namedItem(QString("album")).toElement().text();
    m_title = itemNode.namedItem(QString("title")).toElement().text();
    m_syncronized = (itemNode.namedItem(QString("syncronized")).toElement().text() == "True");
    m_changed = false;

    clearLyrics();

    itemList = itemNode.toElement().elementsByTagName("lyric");

    QStringList lyrics;

    for (int x = 0; x < itemList.count(); x++)
    {
        QDomNode lyricNode = itemList.at(x);
        QString lyric = lyricNode.toElement().text();

        if (m_syncronized)
        {
            QStringList times;
            int lastind = 0;
            while (lyric.indexOf(QRegExp("\\[\\d\\d:\\d\\d\\]"),
                                 lastind) == lastind ||
                   lyric.indexOf(QRegExp("\\[\\d\\d:\\d\\d\\.\\d\\d\\]"),
                                 lastind) == lastind)
            {
                if (lyric[lastind+6] == '.')
                {
                    times.append(lyric.mid(lastind,10));
                    lastind += 10;
                }
                else
                {
                    // short version
                    times.append(lyric.mid(lastind,7));
                    lastind += 7;
                }
            }
            if (!times.isEmpty())
            {
                for (int y = 0; y < times.count(); y++)
                {
                    lyrics.append(times.at(y) + lyric.mid(lastind));
                }
            }
            else
            {
                lyrics.append(lyric);
            }
        }
        else
        {
            lyrics.append(lyric);
        }
    }

    setLyrics(lyrics);

    m_status = STATUS_FOUND;
}

void LyricsData::setLyrics(const QStringList &lyrics)
{
    clearLyrics();

    int lastTime = -1;
    int offset = 0;  // offset in milliseconds

    for (int x = 0; x < lyrics.count(); x++)
    {
        QString lyric = lyrics.at(x);

        auto *line = new LyricsLine;

        if (lyric.startsWith("[offset:"))
        {
            offset = lyric.mid(8,lyric.indexOf("]", 8)-8).toInt();
        }

        if (m_syncronized)
        {
            if (!lyric.isEmpty())
            {
                // does the line start with a time code like [12:34] or [12:34.56]
                if (lyric.indexOf(QRegExp("\\[\\d\\d:\\d\\d\\]"), 0) == 0 ||
                    lyric.indexOf(QRegExp("\\[\\d\\d:\\d\\d\\.\\d\\d\\]"), 0) == 0)
                {
                    int minutes = lyric.mid(1, 2).toInt();
                    int seconds = lyric.mid(4, 2).toInt();
                    int hundredths = 0;
                    if (lyric[6] == '.')
                    {
                        hundredths = lyric.mid(7, 2).toInt();
                        line->m_lyric = lyric.mid(10);
                    }
                    else
                    {
                        line->m_lyric = lyric.mid(7);
                    }
                    line->m_time = (minutes * 60 * 1000) + (seconds * 1000) + (hundredths * 10);
                    if (offset > 0)
                    {
                        if (offset > line->m_time) line->m_time = 0;
                        else line->m_time -= offset;
                    }
                    else
                    {
                        line->m_time -= offset;
                    }
                    lastTime = line->m_time;
                }
                else
                {
                    line->m_time = ++lastTime;
                    line->m_lyric = lyric;
                }
            }
        }
        else
        {
            // synthesize a time code from the track length and the number of lyrics lines
            if (m_parent && !m_parent->isRadio())
            {
                line->m_time = (m_parent->Length() / lyrics.count()) * x;
                line->m_lyric = lyric;
                lastTime = line->m_time;
            }
            else
            {
                line->m_time = ++lastTime;
                line->m_lyric = lyric;
            }
        }

        // ignore anything that is not a lyric
        if (line->m_lyric.startsWith("[ti:") || line->m_lyric.startsWith("[al:") || 
            line->m_lyric.startsWith("[ar:") || line->m_lyric.startsWith("[by:") ||
            line->m_lyric.startsWith("[url:") || line->m_lyric.startsWith("[offset:") ||
            line->m_lyric.startsWith("[id:") || line->m_lyric.startsWith("[length:") ||
            line->m_lyric.startsWith("[au:") || line->m_lyric.startsWith("[la:"))
        {
            delete line;
            continue;
        }

        m_lyricsMap.insert(line->m_time, line);
    }
}
