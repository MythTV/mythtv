#ifndef METADATA_H_
#define METADATA_H_

#include <utility> // for std::pair
#include <vector>

#include <QString>

#include "parentalcontrols.h"

class MSqlQuery;
class MetadataListManager;

enum { VIDEO_YEAR_DEFAULT = 1895 };

const QString VIDEO_SUBTITLE_DEFAULT = "";

struct SortData;

typedef QHash<QString,QString> MetadataMap;

class Metadata
{
  public:
    typedef std::pair<int, QString> genre_entry;
    typedef std::pair<int, QString> country_entry;
    typedef std::pair<int, QString> cast_entry;
    typedef std::vector<genre_entry> genre_list;
    typedef std::vector<country_entry> country_list;
    typedef std::vector<cast_entry> cast_list;

  public:
    class SortKey
    {
      public:
        SortKey();
        SortKey(const SortData &data);
        SortKey(const SortKey &other);
        SortKey &operator=(const SortKey &rhs);
        ~SortKey();

        bool isSet() const;
        void Clear();

      public:
        SortData *m_sd;
    };

  public:
    static SortKey GenerateDefaultSortKey(const Metadata &m, bool ignore_case);
    static int UpdateHashedDBRecord(const QString &hash, const QString &file_name,
                                    const QString &host);
    static QString VideoFileHash(const QString &file_name, const QString &host);
    static QString FilenameToMeta(const QString &file_name, int position);
    static QString TrimTitle(const QString &title, bool ignore_case);

  public:
    Metadata(const QString &filename = QString(),
             const QString &hash = QString(),
             const QString &trailer = QString(),
             const QString &coverfile = QString(),
             const QString &screenshot = QString(),
             const QString &banner = QString(),
             const QString &fanart = QString(),
             const QString &title = QString(),
             const QString &subtitle = QString(),
             const QString &tagline = QString(),
             int year = VIDEO_YEAR_DEFAULT,
             const QDate &releasedate = QDate(),
             const QString &inetref = QString(),
             const QString &homepage = QString(),
             const QString &director = QString(),
             const QString &plot = QString(),
             float userrating = 0.0,
             const QString &rating = QString(),
             int length = 0,
             int season = 0,
             int episode = 0, 
             const QDate &insertdate = QDate(),
             int id = 0,
             ParentalLevel::Level showlevel = ParentalLevel::plLowest,
             int categoryID = 0,
             int childID = -1,
             bool browse = true,
             bool watched = false,
             const QString &playcommand = QString(),
             const QString &category = QString(),
             const genre_list &genres = genre_list(),
             const country_list &countries = country_list(),
             const cast_list &cast = cast_list(),
             const QString &host = "");
    ~Metadata();
    Metadata(MSqlQuery &query);
    Metadata(const Metadata &rhs);
    Metadata &operator=(const Metadata &rhs);

    void toMap(MetadataMap &metadataMap);

    // returns a string to use when sorting
    bool HasSortKey() const;
    const SortKey &GetSortKey() const;
    void SetSortKey(const SortKey &sort_key);

    const QString &GetPrefix() const;
    void SetPrefix(const QString &prefix);

    const QString &GetTitle() const;
    void SetTitle(const QString& title);

    const QString &GetSubtitle() const;
    void SetSubtitle(const QString &subtitle);

    const QString &GetTagline() const;
    void SetTagline(const QString &tagline);

    int GetYear() const;
    void SetYear(int year);

    QDate GetReleaseDate() const;
    void SetReleaseDate(QDate releasedate);

    const QString &GetInetRef() const;
    void SetInetRef(const QString &inetRef);

    const QString &GetHomepage() const;
    void SetHomepage(const QString &homepage);

    const QString &GetDirector() const;
    void SetDirector(const QString &director);

    const QString &GetPlot() const;
    void SetPlot(const QString &plot);

    float GetUserRating() const;
    void SetUserRating(float userRating);

    const QString &GetRating() const;
    void SetRating(const QString &rating);

    int GetLength() const;
    void SetLength(int length);

    int GetSeason() const;
    void SetSeason(int season);

    int GetEpisode() const;
    void SetEpisode(int episode);

    QDate GetInsertdate() const;
    void SetInsertdate(QDate date);

    unsigned int GetID() const;
    void SetID(int id);

    int GetChildID() const;
    void SetChildID(int childID);

    bool GetBrowse() const;
    void SetBrowse(bool browse);

    bool GetWatched() const;
    void SetWatched(bool watched);

    const QString &GetPlayCommand() const;
    void SetPlayCommand(const QString &playCommand);

    ParentalLevel::Level GetShowLevel() const;
    void SetShowLevel(ParentalLevel::Level showLevel);

    const QString &GetHost() const;
    void SetHost(const QString &host);

    const QString &GetFilename() const;
    void SetFilename(const QString &filename);

    const QString &GetHash() const;
    void SetHash(const QString &hash);

    const QString &GetTrailer() const;
    void SetTrailer(const QString &trailer);

    const QString &GetCoverFile() const;
    void SetCoverFile(const QString &coverFile);

    const QString &GetScreenshot() const;
    void SetScreenshot(const QString &screenshot);

    const QString &GetBanner() const;
    void SetBanner(const QString &banner);

    const QString &GetFanart() const;
    void SetFanart(const QString &fanart);

    const QString &GetCategory() const;

    const genre_list &GetGenres() const;
    void SetGenres(const genre_list &genres);

    const cast_list &GetCast() const;
    void SetCast(const cast_list &cast);

    const country_list &GetCountries() const;
    void SetCountries(const country_list &countries);

    int GetCategoryID() const;
    void SetCategoryID(int id);

    void SaveToDatabase();
    void UpdateDatabase();
    // drops the metadata from the DB
    bool DeleteFromDatabase();

//    bool fillDataFromID(const MetadataListManager &cache);
    bool FillDataFromFilename(const MetadataListManager &cache);

    // If you aren't VideoList don't call this
    bool DeleteFile(class VideoList &dummy);

    /// Resets to default metadata
    void Reset();

    bool IsHostSet() const;

  private:
    class MetadataImp *m_imp;
};

void ClearMap(MetadataMap &metadataMap);

bool operator==(const Metadata &a, const Metadata &b);
bool operator!=(const Metadata &a, const Metadata &b);

bool operator<(const Metadata::SortKey &lhs, const Metadata::SortKey &rhs);

#endif
