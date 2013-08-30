#include "cddb.h"

#include <cstddef>
#include <cstdlib>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QVector>
#include <QMap>

#include <mythversion.h>
#include <mythlogging.h>
#include <mythcontext.h>
#include "mythdownloadmanager.h"

/*
 * CDDB protocol docs:
 * http://ftp.freedb.org/pub/freedb/latest/CDDBPROTO
 * http://ftp.freedb.org/pub/freedb/misc/freedb_howto1.07.zip
 * http://ftp.freedb.org/pub/freedb/misc/freedb_CDDB_protcoldoc.zip
 */

const int CDROM_LEADOUT_TRACK = 0xaa;
const int CD_FRAMES_PER_SEC = 75;
const int SECS_PER_MIN = 60;

//static const char URL[] = "http://freedb.freedb.org/~cddb/cddb.cgi?cmd=";
static const char URL[] = "http://freedb.musicbrainz.org/~cddb/cddb.cgi?cmd=";
static const QString& helloID();

namespace {
/*
 * Local cddb database
 */
struct Dbase
{
    static bool Search(Cddb::Matches&, Cddb::discid_t);
    static bool Search(Cddb::Album&, const QString& genre, Cddb::discid_t);
    static bool Write(const Cddb::Album&);

    static void New(Cddb::discid_t, const Cddb::Toc&);
    static void MakeAlias(const Cddb::Album&, const Cddb::discid_t);

private:
    static bool CacheGet(Cddb::Matches&, Cddb::discid_t);
    static bool CacheGet(Cddb::Album&, const QString& genre, Cddb::discid_t);
    static void CachePut(const Cddb::Album&);

    // DiscID to album info cache
    typedef QMap< Cddb::discid_t, Cddb::Album > cache_t; 
    static cache_t s_cache;

    static const QString& GetDB();
};
QMap< Cddb::discid_t, Cddb::Album > Dbase::s_cache;
}


/*
 * Inline helpers
 */
// min/sec/frame to/from lsn
static inline unsigned long msf2lsn(const Cddb::Msf& msf)
{
    return ((unsigned long)msf.min * SECS_PER_MIN + msf.sec) *
        CD_FRAMES_PER_SEC + msf.frame;
}
static inline Cddb::Msf lsn2msf(unsigned long lsn)
{
    Cddb::Msf msf;

    std::div_t d = std::div(lsn, CD_FRAMES_PER_SEC);
    msf.frame = d.rem;
    d = std::div(d.quot, SECS_PER_MIN);
    msf.sec = d.rem;
    msf.min = d.quot;
    return msf;
}

// min/sec/frame to/from seconds
static inline int msf2sec(const Cddb::Msf& msf)
{
    return msf.min * SECS_PER_MIN + msf.sec;
}
static inline Cddb::Msf sec2msf(unsigned sec)
{
    Cddb::Msf msf;

    std::div_t d = std::div(sec, SECS_PER_MIN);
    msf.sec = d.rem;
    msf.min = d.quot;
    msf.frame = 0;
    return msf;
}


/**
 * CDDB query
 */
// static
bool Cddb::Query(Matches& res, const Toc& toc)
{
    if (toc.size() < 2)
        return false;
    const unsigned totalTracks = toc.size() - 1;

    unsigned secs = 0;
    const discid_t discID = Discid(secs, toc.data(), totalTracks);

    // Is it cached?
    if (Dbase::Search(res, discID))
        return res.matches.size() > 0;

    // Construct query
    // cddb query discid ntrks off1 off2 ... nsecs
    QString URL2 = URL + QString("cddb+query+%1+%2+").arg(discID,0,16).arg(totalTracks);

    for (unsigned t = 0; t < totalTracks; ++t)
        URL2 += QString("%1+").arg(msf2lsn(toc[t]));

    URL2 += QString::number(secs);

    // Send the request
    URL2 += "&hello=" + helloID() + "&proto=5";
    LOG(VB_MEDIA, LOG_INFO, "CDDB lookup: " + URL2);

    QString cddb;
    QByteArray data;
    if (!GetMythDownloadManager()->download(URL2, &data))
        return false;
    cddb = data;

    // Check returned status
    const uint stat = cddb.left(3).toUInt(); // Extract 3 digit status:
    cddb = cddb.mid(4);
    switch (stat)
    {
    case 200: // Unique match
        LOG(VB_MEDIA, LOG_INFO, "CDDB match: " + cddb.trimmed());
        // e.g. "200 rock 960b5e0c Nichole Nordeman / Woven & Spun"
        res.discID = discID;
        res.isExact = true;
        res.matches.push_back(Match(
            cddb.section(' ', 0, 0), // genre
            cddb.section(' ', 1, 1).toUInt(0,16), // discID
            cddb.section(' ', 2).section(" / ", 0, 0), // artist
            cddb.section(' ', 2).section(" / ", 1) // title
        ));
        break;

    case 202: // No match for disc ID...
        LOG(VB_MEDIA, LOG_INFO, "CDDB no match");
        Dbase::New(discID, toc); // Stop future lookups
        return false;

    case 210:  // Multiple exact matches
    case 211:  // Multiple inexact matches
        // 210 Found exact matches, list follows (until terminating `.')
        // 211 Found inexact matches, list follows (until terminating `.')
        res.discID = discID;
        res.isExact = 210 == stat;

        // Remove status line
        cddb = cddb.section('\n', 1);

        // Append all matches
        while (!cddb.isEmpty() && !cddb.startsWith("."))
        {
            LOG(VB_MEDIA, LOG_INFO, QString("CDDB %1 match: %2").
                arg(210 == stat ? "exact" : "inexact").
                arg(cddb.section('\n',0,0)));
            res.matches.push_back(Match(
                cddb.section(' ', 0, 0), // genre
                cddb.section(' ', 1, 1).toUInt(0,16), // discID
                cddb.section(' ', 2).section(" / ", 0, 0), // artist
                cddb.section(' ', 2).section(" / ", 1) // title
            ));
            cddb = cddb.section('\n', 1);
        }
        if (res.matches.size() <= 0)
            Dbase::New(discID, toc); // Stop future lookups
        break;

    default:
        // TODO try a (telnet 8880) CDDB lookup
        LOG(VB_GENERAL, LOG_INFO, QString("CDDB query error: %1").arg(stat) +
            cddb.trimmed());
        return false;
    }
    return true;
}

/**
 * CDDB read
 */
// static
bool Cddb::Read(Album& album, const QString& genre, discid_t discID)
{
    // Is it cached?
    if (Dbase::Search(album, genre.toLower(), discID))
        return true;

    // Lookup the details...
    QString URL2 = URL + QString("cddb+read+") + genre.toLower() +
        QString("+%1").arg(discID,0,16) + "&hello=" + helloID() + "&proto=5";
    LOG(VB_MEDIA, LOG_INFO, "CDDB read: " + URL2);

    QString cddb;
    QByteArray data;
    if (!GetMythDownloadManager()->download(URL2, &data))
        return false;
    cddb = data;

    // Check returned status
    const uint stat = cddb.left(3).toUInt(); // Get 3 digit status:
    cddb = cddb.mid(4);
    switch (stat)
    {
    case 210: // OK, CDDB database entry follows (until terminating marker)
        LOG(VB_MEDIA, LOG_INFO, "CDDB read returned: " + cddb.section(' ',0,3));
        LOG(VB_MEDIA, LOG_DEBUG, cddb.section('\n',1).trimmed());
        break;
    default:
        LOG(VB_GENERAL, LOG_INFO, QString("CDDB read error: %1").arg(stat) +
            cddb.trimmed());
        return false;
    }

    album = cddb;
    album.discGenre = genre;
    album.discID = discID;

    // Success - add to cache
    Dbase::Write(album);

    return true;
}

/**
 * CDDB write
 */
// static
bool Cddb::Write(const Album& album, bool /*bLocalOnly =true*/)
{
// TODO send to cddb if !bLocalOnly
    Dbase::Write(album);
    return true;
}

static inline int cddb_sum(int i)
{
    int total = 0;
    while (i > 0)
    {
        const std::div_t d = std::div(i,10);
        total += d.rem;
        i = d.quot;
    }
    return total;
}

/**
 * discID calculation. See appendix A of freedb_howto1.07.zip
 */
// static
Cddb::discid_t Cddb::Discid(unsigned& secs, const Msf v[], unsigned tracks)
{
    int checkSum = 0;
    for (unsigned t = 0; t < tracks; ++t)
        checkSum += cddb_sum(v[t].min * SECS_PER_MIN + v[t].sec);

    secs = v[tracks].min * SECS_PER_MIN + v[tracks].sec -
        (v[0].min * SECS_PER_MIN + v[0].sec);

    const discid_t discID = ((discid_t)(checkSum % 255) << 24) |
        ((discid_t)secs << 8) | tracks;
    return discID;
}

/**
 * Create a local alias for a matched discID
 */
// static
void Cddb::Alias(const Album& album, discid_t discID)
{
    Dbase::MakeAlias(album, discID);
}

/**
 * Parse CDDB text
 */
Cddb::Album& Cddb::Album::operator =(const QString& rhs)
{
    discGenre.clear();
    discID = 0;
    artist.clear();
    title.clear();
    genre.clear();
    year = 0;
    submitter = "MythTV " MYTH_BINARY_VERSION;
    rev = 1;
    isCompilation = false;
    tracks.clear();
    toc.clear();
    extd.clear();
    ext.clear();

    enum { kNorm, kToc } eState = kNorm;

    QString cddb = QString::fromUtf8(rhs.toAscii().constData());
    while (!cddb.isEmpty())
    {
        // Lines should be of the form "FIELD=value\r\n"
        QString line  = cddb.section(QRegExp("[\r\n]"), 0, 0);

        if (line.startsWith("# Track frame offsets:"))
        {
            eState = kToc;
        }
        else if (line.startsWith("# Disc length:"))
        {
            QString s = line.section(QRegExp("[ \t]"), 3, 3);
            unsigned secs = s.toULong();
            if (toc.size())
                secs -= msf2sec(toc[0]);
            toc.push_back(sec2msf(secs));
            eState = kNorm;
        }
        else if (line.startsWith("# Revision:"))
        {
            QString s = line.section(QRegExp("[ \t]"), 2, 2);
            bool bValid = false;
            int v = s.toInt(&bValid);
            if (bValid)
                rev = v;
        }
        else if (line.startsWith("# Submitted via:"))
        {
            submitter = line.section(QRegExp("[ \t]"), 3, 3);
        }
        else if (line.startsWith("#"))
        {
            if (kToc == eState)
            {
                bool bValid = false;
                QString s = line.section(QRegExp("[ \t]"), 1).trimmed();
                unsigned long lsn = s.toUInt(&bValid);
                if (bValid)
                    toc.push_back(lsn2msf(lsn));
                else
                    eState = kNorm;
            }
        }
        else
        {
            QString value = line.section('=', 1, 1);
            QString art;

            if (value.contains(" / "))
            {
                art   = value.section(" / ", 0, 0);  // Artist in *TITLE
                value = value.section(" / ", 1, 1);
            }

            if (line.startsWith("DISCID="))
            {
                bool isValid = false;
                ulong discID2 = value.toULong(&isValid,16);
                if (isValid)
                    discID = discID2;
            }
            else if (line.startsWith("DTITLE="))
            {
                // Albums (and maybe artists?) can wrap over multiple lines:
                artist += art;
                title  += value;
            }
            else if (line.startsWith("DYEAR="))
            {
                bool isValid = false;
                int val = value.toInt(&isValid);
                if (isValid)
                    year = val;
            }
            else if (line.startsWith("DGENRE="))
            {
                if (!value.isEmpty())
                    genre = value;
            }
            else if (line.startsWith("TTITLE"))
            {
                int trk = line.remove("TTITLE").section('=', 0, 0).toInt();
                if (trk >= 0 && trk < CDROM_LEADOUT_TRACK)
                {
                    if (trk >= tracks.size())
                        tracks.resize(trk + 1);

                    Cddb::Track& track = tracks[trk];

                    // Titles can wrap over multiple lines, so we load+store:
                    track.title += value;
                    track.artist += art;

                    if (art.length())
                        isCompilation = true;
                }
            }
            else if (line.startsWith("EXTD="))
            {
                if (!value.isEmpty())
                    extd = value;
            }
            else if (line.startsWith("EXTT"))
            {
                int trk = line.remove("EXTT").section('=', 0, 0).toInt();
                if (trk >= 0 && trk < CDROM_LEADOUT_TRACK)
                {
                    if (trk >= ext.size())
                        ext.resize(trk + 1);

                    ext[trk] = value;
                }
            }
        }

        // Next response line:
        cddb = cddb.section('\n', 1);
    }
    return *this;
}

/**
 * Convert album to CDDB text form
 */
Cddb::Album::operator QString() const
{
    QString ret = "# xmcd\n"
        "#\n"
        "# Track frame offsets:\n";
    for (int i = 1; i < toc.size(); ++i)
        ret += "#       " + QString::number(msf2lsn(toc[i - 1])) + '\n';
    ret += "#\n";
    ret += "# Disc length: " +
        QString::number( msf2sec(toc.last()) + msf2sec(toc[0]) ) +
        " seconds\n";
    ret += "#\n";
    ret += "# Revision: " + QString::number(rev) + '\n';
    ret += "#\n";
    ret += "# Submitted via: " + (!submitter.isEmpty() ? submitter :
            "MythTV " MYTH_BINARY_VERSION) + '\n';
    ret += "#\n";
    ret += "DISCID=" + QString::number(discID,16) + '\n';
    ret += "DTITLE=" + artist.toUtf8() + " / " + title + '\n';
    ret += "DYEAR=" + (year ? QString::number(year) : "")+ '\n';
    ret += "DGENRE=" + genre.toUtf8() + '\n';
    for (int t = 0; t < tracks.size(); ++t)
        ret += "TTITLE" + QString::number(t) + "=" +
            tracks[t].title.toUtf8() + '\n';
    ret += "EXTD=" + extd.toUtf8() + '\n';
    for (int t = 0; t < tracks.size(); ++t)
        ret += "EXTT" + QString::number(t) + "=" + ext[t].toUtf8() + '\n';
    ret += "PLAYORDER=\n";

    return ret;
}


/**********************************************************
 * Local cddb database ops
 **********************************************************/

// search local database for discID
bool Dbase::Search(Cddb::Matches& res, const Cddb::discid_t discID)
{
    res.matches.empty();
    res.discID = discID;

    if (CacheGet(res, discID))
        return true;

    QFileInfoList list = QDir(GetDB()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (QFileInfoList::const_iterator it = list.begin(); it != list.end(); ++it)
    {
        QString genre = it->baseName();

        QFileInfoList ids = QDir(it->canonicalFilePath()).entryInfoList(QDir::Files);
        for (QFileInfoList::const_iterator it2 = ids.begin(); it2 != ids.end(); ++it2)
        {
            if (it2->baseName().toUInt(0,16) == discID)
            {
                QFile file(it2->canonicalFilePath());
                if (file.open(QIODevice::ReadOnly | QIODevice::Text))
                {
                    Cddb::Album a = QTextStream(&file).readAll();
                    a.discGenre = genre;
                    a.discID = discID;
                    LOG(VB_MEDIA, LOG_INFO, QString("LocalCDDB found %1 in ").
                        arg(discID,0,16) + genre + " : " +
                        a.artist + " / " + a.title);

                    CachePut(a);
                    res.matches.push_back(Cddb::Match(genre,discID,a.artist,a.title));
                }

            }
        }
    }
    return res.matches.size() > 0;
}

// search local database for genre/discID
bool Dbase::Search(Cddb::Album& a, const QString& genre, const Cddb::discid_t discID)
{
    if (CacheGet(a, genre, discID))
        return true;

    QFile file(GetDB() + '/' + genre.toLower() + '/' + QString::number(discID,16));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        a = QTextStream(&file).readAll();
        a.discGenre = genre.toLower();
        a.discID = discID;
        LOG(VB_MEDIA, LOG_INFO, QString("LocalCDDB matched %1 ").arg(discID,0,16) +
            genre + " to " + a.artist + " / " + a.title);

        CachePut(a);

        return true;
    }
    return false;
}

// Create CDDB file
bool Dbase::Write(const Cddb::Album& album)
{
    CachePut(album);

    const QString genre = !album.discGenre.isEmpty() ?
        album.discGenre.toLower().toUtf8() : "misc";

    LOG(VB_MEDIA, LOG_INFO, "WriteDB " + genre +
        QString(" %1 ").arg(album.discID,0,16) +
        album.artist + " / " + album.title);

    if (QDir(GetDB()).mkpath(genre))
    {
        QFile file(GetDB() + '/' + genre + '/' + 
            QString::number(album.discID,16));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream(&file) << album;
            return true;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, "Cddb can't write " + file.fileName());
    }
    else
        LOG(VB_GENERAL, LOG_ERR, "Cddb can't mkpath " + GetDB() + '/' + genre);
    return false;
}

// Create a local alias for a matched discID
// static
void Dbase::MakeAlias(const Cddb::Album& album, const Cddb::discid_t discID)
{
    LOG(VB_MEDIA, LOG_DEBUG, QString("Cddb MakeAlias %1 for %2 ")
        .arg(discID,0,16).arg(album.discID,0,16)
        + album.genre + " " + album.artist + " / " + album.title);
    s_cache.insert(discID, album)->discID = discID;
}

// Create a new entry for a discID
// static
void Dbase::New(const Cddb::discid_t discID, const Cddb::Toc& toc)
{
    s_cache.insert(discID, Cddb::Album(discID))->toc = toc;
}

// static
void Dbase::CachePut(const Cddb::Album& album)
{
    LOG(VB_MEDIA, LOG_DEBUG, QString("Cddb CachePut %1 ")
        .arg(album.discID,0,16)
        + album.genre + " " + album.artist + " / " + album.title);
    s_cache.insertMulti(album.discID, album);
}

// static
bool Dbase::CacheGet(Cddb::Matches& res, const Cddb::discid_t discID)
{
    bool ret = false;
    for (cache_t::const_iterator it = s_cache.find(discID); it != s_cache.end(); ++it)
    {
        if (it->discID == discID)
        {
            ret = true;
            res.discID = discID;
            LOG(VB_MEDIA, LOG_DEBUG, QString("Cddb CacheGet %1 found %2 ")
                .arg(discID,0,16).arg(it->discID,0,16)
                + it->discGenre + " " + it->artist + " / " + it->title);

            // If it's marker for 'no match' then genre is empty
            if (!it->discGenre.isEmpty())
                res.matches.push_back(Cddb::Match(*it));
        }
    }
    return ret;
}

// static
bool Dbase::CacheGet(Cddb::Album& album, const QString& genre, const Cddb::discid_t discID)
{
    const Cddb::Album& a = s_cache[ discID];
    if (a.discID && a.discGenre == genre)
    {
        album = a;
        return true;
    }
    return false;
}

// Local database path
// static
const QString& Dbase::GetDB()
{
    static QString s_path;
    if (s_path.isEmpty())
    {
        s_path = getenv("HOME");
#ifdef WIN32
        if (s_path.isEmpty())
        {
            s_path = getenv("HOMEDRIVE");
            s_path += getenv("HOMEPATH");
        }
#endif
        if (s_path.isEmpty())
            s_path = ".";
        if (!s_path.endsWith('/'))
            s_path += '/';
        s_path += ".cddb/";
    }
    return s_path;
}

// CDDB hello string
static const QString& helloID()
{
    static QString helloID;
    if (helloID.isEmpty())
    {
        helloID = getenv("USER");
        if (helloID.isEmpty())
            helloID = "anon";
        helloID += QString("+%1+MythTV+%2+")
                   .arg(gCoreContext->GetHostName()).arg(MYTH_BINARY_VERSION);
    }
    return helloID;
}
