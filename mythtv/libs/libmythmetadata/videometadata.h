#ifndef VIDEOMETADATA_H_
#define VIDEOMETADATA_H_

#include <utility> // for std::pair
#include <vector>

#include <QString>
#include <QDate>
#include <QCoreApplication>

#include "parentalcontrols.h"
#include "mythmetaexp.h"
#include "metadatacommon.h"

class MSqlQuery;
class VideoMetadataListManager;

static constexpr uint16_t VIDEO_YEAR_DEFAULT { 1895 };

const QString VIDEO_SUBTITLE_DEFAULT = "";

using MetadataMap = QHash<QString,QString>;

class META_PUBLIC VideoMetadata
{
    Q_DECLARE_TR_FUNCTIONS(VideoMetadata);

  public:
    using genre_entry = std::pair<int, QString>;
    using country_entry = std::pair<int, QString>;
    using cast_entry = std::pair<int, QString>;
    using genre_list = std::vector<genre_entry>;
    using country_list = std::vector<country_entry>;
    using cast_list = std::vector<cast_entry>;

  public:
    static int UpdateHashedDBRecord(const QString &hash, const QString &file_name,
                                    const QString &host);
    static QString VideoFileHash(const QString &file_name, const QString &host);
    static QString FilenameToMeta(const QString &file_name, int position);
    static QString TrimTitle(const QString &title, bool ignore_case);

  public:
    explicit VideoMetadata(const QString &filename = QString(),
             const QString &sortFilename = QString(),
             const QString &hash = QString(),
             const QString &trailer = QString(),
             const QString &coverfile = QString(),
             const QString &screenshot = QString(),
             const QString &banner = QString(),
             const QString &fanart = QString(),
             const QString &title = QString(),
             const QString &sortTitle = QString(),
             const QString &subtitle = QString(),
             const QString &sortSubtitle = QString(),
             const QString &tagline = QString(),
             int year = VIDEO_YEAR_DEFAULT,
             QDate releasedate = QDate(),
             const QString &inetref = QString(),
             int collectionref = 0,
             const QString &homepage = QString(),
             const QString &director = QString(),
             const QString &studio = QString(),
             const QString &plot = QString(),
             float userrating = 0.0,
             const QString &rating = QString(),
             int length = 0,
             int playcount = 0,
             int season = 0,
             int episode = 0,
             QDate insertdate = QDate(),
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
             const QString &host = "",
             bool processed = false,
             VideoContentType contenttype = kContentUnknown);
    ~VideoMetadata();
    explicit VideoMetadata(MSqlQuery &query);
    VideoMetadata(const VideoMetadata &rhs);
    VideoMetadata &operator=(const VideoMetadata &rhs);
    bool sortBefore(const VideoMetadata &rhs) const;

    void toMap(InfoMap &metadataMap);
    QString GetText(const QString& name) const;
    void GetStateMap(InfoMap &stateMap) const;
    QString GetState(const QString& name) const;
    void GetImageMap(InfoMap &imageMap);
    QString GetImage(const QString& name) const;

    static QString MetadataGetTextCb(const QString& name, void *data);
    static QString MetadataGetImageCb(const QString& name, void *data);
    static QString MetadataGetStateCb(const QString& name, void *data);

    const QString &GetPrefix() const;
    void SetPrefix(const QString &prefix);

    const QString &GetTitle() const;
    const QString &GetSortTitle() const;
    void SetTitle(const QString& title, const QString& sortTitle = "");

    const QString &GetSubtitle() const;
    const QString &GetSortSubtitle() const;
    void SetSubtitle(const QString &subtitle, const QString &sortSubtitle = "");

    const QString &GetTagline() const;
    void SetTagline(const QString &tagline);

    int GetYear() const;
    void SetYear(int year);

    QDate GetReleaseDate() const;
    void SetReleaseDate(QDate releasedate);

    const QString &GetInetRef() const;
    void SetInetRef(const QString &inetRef);

    int GetCollectionRef() const;
    void SetCollectionRef(int collectionref);

    const QString &GetHomepage() const;
    void SetHomepage(const QString &homepage);

    const QString &GetDirector() const;
    void SetDirector(const QString &director);

    const QString &GetStudio() const;
    void SetStudio(const QString &studio);

    const QString &GetPlot() const;
    void SetPlot(const QString &plot);

    float GetUserRating() const;
    void SetUserRating(float userRating);

    const QString &GetRating() const;
    void SetRating(const QString &rating);

    std::chrono::minutes GetLength() const;
    void SetLength(std::chrono::minutes length);

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

    bool GetProcessed() const;
    void SetProcessed(bool processed);

    VideoContentType GetContentType() const;
    void SetContentType(VideoContentType contenttype);

    const QString &GetPlayCommand() const;
    void SetPlayCommand(const QString &playCommand);

    unsigned int GetPlayCount() const;
    void SetPlayCount(int count);

    ParentalLevel::Level GetShowLevel() const;
    void SetShowLevel(ParentalLevel::Level showLevel);

    const QString &GetHost() const;
    void SetHost(const QString &host);

    const QString &GetFilename() const;
    const QString &GetSortFilename() const;
    void SetFilename(const QString &filename, const QString &sortFilename = "");

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

//    bool fillDataFromID(const VideoMetadataListManager &cache);
    bool FillDataFromFilename(const VideoMetadataListManager &cache);

    // If you aren't VideoList don't call this
    bool DeleteFile();

    /// Resets to default metadata
    void Reset();

    bool IsHostSet() const;

  private:
    class VideoMetadataImp *m_imp;
};

META_PUBLIC void ClearMap(InfoMap &metadataMap);

META_PUBLIC bool operator==(const VideoMetadata &a, const VideoMetadata &b);
META_PUBLIC bool operator!=(const VideoMetadata &a, const VideoMetadata &b);

Q_DECLARE_METATYPE(VideoMetadata*)

#endif
