#ifndef CDDB_H_
#define CDDB_H_

#include <utility>

#include <QString>
#include <QStringList>
#include <QVector>

/*
 * CDDB lookup
 */
struct Cddb
{
    using discid_t = unsigned long;
    struct Album;

    // A CDDB query match
    struct Match
    {
        QString discGenre;
        discid_t discID { 0 };
        QString artist;
        QString title;

        Match() = default;
        Match(const char *g, discid_t d, const char *a, const char *t) :
            discGenre(g), discID(d), artist(a), title(t)
        {}
        Match(QString g, discid_t d, QString a, QString t) :
            discGenre(std::move(g)), discID(d), artist(std::move(a)), title(std::move(t))
        {}
        explicit Match(const Album& a) : discGenre(a.discGenre), discID(a.discID),
            artist(a.artist), title(a.title)
        {}
    };

    // CDDB query results
    struct Matches
    {
        discid_t discID  {     0 }; // discID of query
        bool     isExact { false };
        using    match_t = QVector< Match >;
        match_t  matches;

        Matches() = default;
    };

    struct Msf
    {
        int min, sec, frame;
        explicit Msf(int m = 0, int s = 0, int f = 0) : min(m), sec(s), frame(f) {}
    };
    using Toc = QVector< Msf >;

    struct Track
    {
        QString artist;
        QString title;
    };

    // CDDB detail result
    struct Album
    {
        QString discGenre; // the genre used in the query to differentiate similar discID's
        discid_t discID       { 0 };
        QString artist;
        QString title;
        QString genre;     // the genre from the DGENRE= item
        int     year          { 0 };
        QString submitter;
        int     rev           { 1 };
        bool    isCompilation { false };
        using track_t = QVector< Track >;
        track_t tracks;
        QString extd;
        using ext_t = QVector< QString >;
        ext_t ext;
        Toc toc;

        explicit Album(discid_t d = 0, const char* g = nullptr) :
            discGenre(g), discID(d) {}

        explicit Album(const QString& s) { *this = s; }

        Album& operator = (const QString& rhs);
        operator QString () const;
    };

    // Primary cddb access
    static bool Query(Matches& res, const Toc& toc);
    static bool Read(Album& album, const QString& genre, discid_t discID);
    static bool Write(const Album& album, bool bLocalOnly = true);

    // Support
    static discid_t Discid(unsigned& secs, const Msf v[], unsigned tracks);
    static void Alias(const Album& album, discid_t discID);
};

#endif //ndef CDDB_H_
