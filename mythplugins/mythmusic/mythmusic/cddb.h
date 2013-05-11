#ifndef CDDB_H_
#define CDDB_H_

#include <QString>
#include <QStringList>
#include <QVector>

/*
 * CDDB lookup
 */
struct Cddb
{
    typedef unsigned long discid_t;
    struct Album;

    // A CDDB query match
    struct Match
    {
        QString discGenre;
        discid_t discID;
        QString artist;
        QString title;

        Match() : discID(0) {}
        Match(const char *g, discid_t d, const char *a, const char *t) :
            discGenre(g), discID(d), artist(a), title(t)
        {}
        Match(const QString &g, discid_t d, const QString &a, const QString &t) :
            discGenre(g), discID(d), artist(a), title(t)
        {}
        Match(const Album& a) : discGenre(a.discGenre), discID(a.discID),
            artist(a.artist), title(a.title)
        {}
    };

    // CDDB query results
    struct Matches
    {
        discid_t discID; // discID of query
        bool isExact;
        typedef QVector< Match > match_t;
        match_t matches;

        Matches() : discID(0), isExact(false) {}
    };

    struct Msf
    {
        int min, sec, frame;
        Msf(int m = 0, int s = 0, int f = 0) : min(m), sec(s), frame(f) {}
    };
    typedef QVector< Msf > Toc;

    struct Track
    {
        QString artist;
        QString title;
    };

    // CDDB detail result
    struct Album
    {
        QString discGenre; // the genre used in the query to differentiate similar discID's
        discid_t discID;
        QString artist;
        QString title;
        QString genre;     // the genre from the DGENRE= item
        int year;
        QString submitter;
        int rev;
        bool isCompilation;
        typedef QVector< Track > track_t;
        track_t tracks;
        QString extd;
        typedef QVector< QString > ext_t;
        ext_t ext;
        Toc toc;

        Album(discid_t d = 0, const char* g = 0) :
            discGenre(g), discID(d), year(0), rev(1), isCompilation(false) {}

        Album(const QString& s) { *this = s; }

        Album& operator = (const QString&);
        operator QString () const;
    };

    // Primary cddb access
    static bool Query(Matches&, const Toc&);
    static bool Read(Album&, const QString& genre, discid_t);
    static bool Write(const Album&, bool bLocalOnly = true);

    // Support
    static discid_t Discid(unsigned& secs, const Msf [], unsigned tracks);
    static void Alias(const Album&, discid_t);
};

#endif //ndef CDDB_H_
