#ifndef MUSICMETADATA_H_
#define MUSICMETADATA_H_

// C/C++
#include <array>
#include <cstdint>
#include <utility>

// qt
#include <QCoreApplication>
#include <QDateTime>
#include <QImage>
#include <QMap>
#include <QMetaType>
#include <QStringList>
#include <QTimeZone>

// MythTV
#include "libmythbase/mthread.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythtypes.h"
#include "libmythmetadata/mythmetaexp.h"

class AllMusic;
class AlbumArtImages;
class LyricsData;
class MetaIO;

enum ImageType : std::uint8_t
{
    IT_UNKNOWN = 0,
    IT_FRONTCOVER,
    IT_BACKCOVER,
    IT_CD,
    IT_INLAY,
    IT_ARTIST,
    IT_LAST
};

class META_PUBLIC AlbumArtImage
{
  public:
    AlbumArtImage(void) :
            m_filename(""), m_hostname(""), m_description("") {}
    explicit AlbumArtImage(const AlbumArtImage * const image) :
            m_id(image->m_id), m_filename(image->m_filename),
            m_hostname(image->m_hostname), m_imageType(image->m_imageType),
            m_description(image->m_description), m_embedded(image->m_embedded) {}
     int       m_id          {0};
     QString   m_filename;
     QString   m_hostname;
     ImageType m_imageType   {IT_UNKNOWN};
     QString   m_description;
     bool      m_embedded    {false};
};

using AlbumArtList = QList<AlbumArtImage*>;

enum RepoType : std::uint8_t
{
    RT_Database = 0,
    RT_CD       = 1,
    RT_Radio    = 2
};

static constexpr uint8_t  METADATA_BITS_FOR_REPO {  8 };
static constexpr uint8_t  METADATA_REPO_SHIFT    { 24 };
static constexpr uint32_t METADATA_REPO_MASK     { 0xff000000 };
static constexpr uint32_t METADATA_ID_MASK       { 0x00ffffff };

static constexpr uint32_t ID_TO_ID(uint32_t x) { return x & METADATA_ID_MASK; };
static constexpr uint32_t ID_TO_REPO(uint32_t x) { return x >> METADATA_REPO_SHIFT; };

static constexpr const char* METADATA_INVALID_FILENAME { "**NOT FOUND**" };

static constexpr const char* STREAMUPDATEURL { "https://services.mythtv.org/music/data/?data=streams" };
static constexpr size_t STREAMURLCOUNT { 5 };

using UrlList = std::array<QString,STREAMURLCOUNT>;

class META_PUBLIC MusicMetadata
{
    Q_DECLARE_TR_FUNCTIONS(MusicMetadata);

  public:

    using IdType = uint32_t;

    explicit MusicMetadata(QString lfilename = "", QString lartist = "", QString lcompilation_artist = "",
             QString lalbum = "", QString ltitle = "", QString lgenre = "",
             int lyear = 0, int ltracknum = 0, std::chrono::milliseconds llength = 0ms, int lid = 0,
             int lrating = 0, int lplaycount = 0, QDateTime llastplay = QDateTime(),
             QDateTime ldateadded = QDateTime(), bool lcompilation = false, QString lformat = "")
                : m_artist(std::move(lartist)),
                   m_compilationArtist(std::move(lcompilation_artist)),
                   m_album(std::move(lalbum)),
                   m_title(std::move(ltitle)),
                   m_genre(std::move(lgenre)),
                   m_format(std::move(lformat)),
                   m_year(lyear),
                   m_trackNum(ltracknum),
                   m_length(llength),
                   m_rating(lrating),
                   m_lastPlay(std::move(llastplay)),
                   m_dateAdded(std::move(ldateadded)),
                   m_playCount(lplaycount),
                   m_compilation(lcompilation),
                   m_id(lid),
                   m_filename(std::move(lfilename))
    {
        checkEmptyFields();
    }

    MusicMetadata(int lid, QString lbroadcaster, QString lchannel, QString ldescription, const UrlList &lurls, QString llogourl,
             QString lgenre, QString lmetaformat, QString lcountry, QString llanguage, QString lformat);

    ~MusicMetadata();

    MusicMetadata(const MusicMetadata &other)
    {
        *this = other;
    }

    MusicMetadata& operator=(const MusicMetadata &rhs);

    QString Artist() const { return m_artist; }
    QString ArtistSort() const { return m_artistSort; }
    void setArtist(const QString &lartist,
                   const QString &lartist_sort = nullptr)
    {
        m_artist = lartist;
        m_artistId = -1;
        m_artistSort = lartist_sort;
        m_formattedArtist.clear(); m_formattedTitle.clear();
        ensureSortFields();
    }

    QString CompilationArtist() const { return m_compilationArtist; }
    QString CompilationArtistSort() const { return m_compilationArtistSort; }
    void setCompilationArtist(const QString &lcompilation_artist,
                              const QString &lcompilation_artist_sort = nullptr)
    {
        m_compilationArtist = lcompilation_artist;
        m_compartistId = -1;
        m_compilationArtistSort = lcompilation_artist_sort;
        m_formattedArtist.clear(); m_formattedTitle.clear();
        ensureSortFields();
    }

    QString Album() const { return m_album; }
    QString AlbumSort() const { return m_albumSort; }
    void setAlbum(const QString &lalbum,
                  const QString &lalbum_sort = nullptr)
    {
        m_album = lalbum;
        m_albumId = -1;
        m_albumSort = lalbum_sort;
        m_formattedArtist.clear(); m_formattedTitle.clear();
        ensureSortFields();
    }

    QString Title() const { return m_title; }
    QString TitleSort() const { return m_titleSort; }
    void setTitle(const QString &ltitle,
                  const QString &ltitle_sort = nullptr)
    {
        m_title = ltitle;
        m_titleSort = ltitle_sort;
        ensureSortFields();
    }

    QString FormatArtist();
    QString FormatTitle();

    QString Genre() const { return m_genre; }
    void setGenre(const QString &lgenre) {
        m_genre = lgenre;
        m_genreId = -1;
    }

    void setDirectoryId(int ldirectoryid) { m_directoryId = ldirectoryid; }
    int getDirectoryId();

    void setArtistId(int lartistid) { m_artistId = lartistid; }
    int getArtistId();

    void setCompilationArtistId(int lartistid) { m_compartistId = lartistid; }
    int getCompilationArtistId();

    void setAlbumId(int lalbumid) { m_albumId = lalbumid; }
    int getAlbumId();

    void setGenreId(int lgenreid) { m_genreId = lgenreid; }
    int getGenreId();

    int Year() const { return m_year; }
    void setYear(int lyear) { m_year = lyear; }

    int Track() const { return m_trackNum; }
    void setTrack(int ltrack) { m_trackNum = ltrack; }

    int GetTrackCount() const { return m_trackCount; }
    void setTrackCount(int ltrackcount) { m_trackCount = ltrackcount; }

    std::chrono::milliseconds Length() const { return m_length; }
    template <typename T, std::enable_if_t<std::chrono::__is_duration<T>::value, bool> = true>
    void setLength(T llength) { m_length = llength; }

    int DiscNumber() const {return m_discNum;}
    void setDiscNumber(int discnum) { m_discNum = discnum; }

    int DiscCount() const {return m_discCount;}
    void setDiscCount(int disccount) { m_discCount = disccount; }

    int Playcount() const { return m_playCount; }
    void setPlaycount(int lplaycount) { m_playCount = lplaycount; }

    IdType ID() const { return m_id; }
    void setID(IdType lid) { m_id = lid; }
    void setRepo(RepoType repo) { m_id = (m_id & METADATA_ID_MASK) | (repo << METADATA_REPO_SHIFT); }

    bool isCDTrack(void) const { return ID_TO_REPO(m_id) == RT_CD; }
    bool isDBTrack(void) const { return ID_TO_REPO(m_id) == RT_Database; }
    bool isRadio(void) const { return ID_TO_REPO(m_id) == RT_Radio; }

    QString Filename(bool find = true);
    void setFilename(const QString &lfilename);
    QString getLocalFilename(void);

    QString Hostname(void) { return m_hostname; }
    void setHostname(const QString &host) { m_hostname = host; }

    uint64_t FileSize() const { return m_fileSize; }
    void setFileSize(uint64_t lfilesize) { m_fileSize = lfilesize; }

    QString Format() const { return m_format; }
    void setFormat(const QString &lformat) { m_format = lformat; }

    int Rating() const { return m_rating; }
    void decRating();
    void incRating();
    void setRating(int lrating) { m_rating = lrating; }

    QDateTime LastPlay() const { return m_lastPlay; }
    void setLastPlay();
    void setLastPlay(const QDateTime& lastPlay);

    int PlayCount() const { return m_playCount; }
    void incPlayCount();

    // track is part of a compilation album
    bool Compilation() const { return m_compilation; }
    void setCompilation(bool state)
    {
        m_compilation = state;
        m_formattedArtist.clear();
        m_formattedTitle.clear();
    }
    bool determineIfCompilation(bool cd = false);

    // for radio streams
    void setBroadcaster(const QString &broadcaster) { m_broadcaster = broadcaster; }
    QString Broadcaster(void) { return m_broadcaster; }

    void setChannel(const QString &channel) { m_channel = channel; }
    QString Channel(void) { return m_channel; }

    void setDescription(const QString &description) { m_description = description; }
    QString Description(void) { return m_description; }

    void setUrl(const QString &url, size_t index = 0);
    QString Url(size_t index = 0);

    void setLogoUrl(const QString &logourl) { m_logoUrl = logourl; }
    QString LogoUrl(void) { return m_logoUrl; }

    void setMetadataFormat(const QString &metaformat) { m_metaFormat = metaformat; }
    QString MetadataFormat(void) { return m_metaFormat; }

    void setCountry(const QString &country) { m_country = country; }
    QString Country(void) { return m_country; }

    void setLanguage(const QString &language) { m_language = language; }
    QString Language(void) { return m_language; }

    void setEmbeddedAlbumArt(AlbumArtList &albumart);

    void reloadMetadata(void);
    void dumpToDatabase(void);
    void setField(const QString &field, const QString &data);
    void getField(const QString& field, QString *data);
    void toMap(InfoMap &metadataMap, const QString &prefix = "");

    void persist(void);

    bool hasChanged(void) const { return m_changed; }

    bool compare(MusicMetadata *mdata) const;

    // static functions
    static MusicMetadata *createFromFilename(const QString &filename);
    static MusicMetadata *createFromID(int trackid);
    static void setArtistAndTrackFormats();
    static QStringList fillFieldList(const QString& field);
    static bool updateStreamList(void);

    // this looks for any image available - preferring a front cover if available
    QString getAlbumArtFile(void);
    // this looks only for the given image type
    QString getAlbumArtFile(ImageType type);

    AlbumArtImages *getAlbumArtImages(void);
    void reloadAlbumArtImages(void);

    LyricsData *getLyricsData(void);

    MetaIO *getTagger(void);

  private:
    void setCompilationFormatting(bool cd = false);
    QString formatReplaceSymbols(const QString &format);
    void checkEmptyFields(void);
    void ensureSortFields(void);
    void saveHostname(void);

    QString m_artist;
    QString m_artistSort;
    QString m_compilationArtist;
    QString m_compilationArtistSort;
    QString m_album;
    QString m_albumSort;
    QString m_title;
    QString m_titleSort;
    QString m_formattedArtist;
    QString m_formattedTitle;
    QString m_genre;
    QString m_format;
    int     m_year             {0};
    int     m_trackNum         {0};
    int     m_trackCount       {0};
    int     m_discNum          {0};
    int     m_discCount        {0};
    std::chrono::milliseconds  m_length  {0ms};
    int     m_rating           {0};
    int     m_directoryId      {-1};
    int     m_artistId         {-1};
    int     m_compartistId     {-1};
    int     m_albumId          {-1};
    int     m_genreId          {-1};
    QDateTime m_lastPlay;
    QDateTime m_tempLastPlay;
    QDateTime m_dateAdded;
    int  m_playCount           {0};
    int  m_tempPlayCount       {0};
    bool m_compilation         {false};

    AlbumArtImages *m_albumArt {nullptr};

    LyricsData *m_lyricsData   {nullptr};

    IdType   m_id              {0};
    QString  m_filename;       // file name as stored in the DB
    QString  m_hostname;       // host where file is located as stored in the DB
    QString  m_actualFilename; // actual URL of the file if found
    uint64_t m_fileSize        {0};
    bool     m_changed         {false};

    // radio stream stuff
    QString m_broadcaster;
    QString m_channel;
    QString m_description;
    UrlList m_urls;
    QString m_logoUrl;
    QString m_metaFormat;
    QString m_country;
    QString m_language;

    // Various formatting strings
    static QString s_formatNormalFileArtist;
    static QString s_formatNormalFileTrack;
    static QString s_formatNormalCdArtist;
    static QString s_formatNormalCdTrack;

    static QString s_formatCompilationFileArtist;
    static QString s_formatCompilationFileTrack;
    static QString s_formatCompilationCdArtist;
    static QString s_formatCompilationCdTrack;
};

bool operator==(MusicMetadata& a, MusicMetadata& b);
bool operator!=(MusicMetadata& a, MusicMetadata& b);

Q_DECLARE_METATYPE(MusicMetadata *)
Q_DECLARE_METATYPE(MusicMetadata)

using MetadataPtrList = QList<MusicMetadata*>;
Q_DECLARE_METATYPE(MetadataPtrList *)
Q_DECLARE_METATYPE(ImageType);

//---------------------------------------------------------------------------

class META_PUBLIC MetadataLoadingThread : public MThread
{

  public:

    explicit MetadataLoadingThread(AllMusic *parent_ptr)
        : MThread("MetadataLoading"), m_parent(parent_ptr) {}
    void run() override; // MThread

  private:

    AllMusic *m_parent {nullptr};
};

//---------------------------------------------------------------------------

class META_PUBLIC AllMusic
{
    Q_DECLARE_TR_FUNCTIONS(AllMusic);

  public:

    AllMusic(void);
    ~AllMusic();

    MusicMetadata*   getMetadata(int an_id);
    bool        updateMetadata(int an_id, MusicMetadata *the_track);
    int         count() const { return m_numPcs; }
    int         countLoaded() const { return m_numLoaded; }
    void        save();
    bool        startLoading(void);
    void        resync();   //  After a CD rip, for example

    // cd stuff
    void        clearCDData(void);
    void        addCDTrack(const MusicMetadata &the_track);
    bool        checkCDTrack(MusicMetadata *the_track);
    MusicMetadata*   getCDMetadata(int m_the_track);
    QString     getCDTitle(void) const { return m_cdTitle; }
    void        setCDTitle(const QString &a_title) { m_cdTitle = a_title; }
    int         getCDTrackCount(void) const { return m_cdData.count(); }

    bool        doneLoading() const { return m_doneLoading; }
    bool        cleanOutThreads();

    MetadataPtrList *getAllMetadata(void) { return &m_allMusic; }
    MetadataPtrList *getAllCDMetadata(void) { return &m_cdData; }

    bool isValidID(int an_id);

  private:
    MetadataPtrList     m_allMusic;

    int m_numPcs                               {0};
    int m_numLoaded                            {0};

    using MusicMap = QMap<int, MusicMetadata*>;
    MusicMap m_musicMap;

    // cd stuff
    MetadataPtrList m_cdData; //  More than one cd player?
    QString m_cdTitle;

    MetadataLoadingThread   *m_metadataLoader  {nullptr};
    bool                     m_doneLoading     {false};

    int                      m_playCountMin    {0};
    int                      m_playCountMax    {0};
    qint64                   m_lastPlayMin     {0};
    qint64                   m_lastPlayMax     {0};
};

using StreamList = QList<MusicMetadata*>;

class META_PUBLIC AllStream
{
  public:

    AllStream(void);
    ~AllStream();

    void loadStreams(void);

    bool isValidID(MusicMetadata::IdType an_id);

    MusicMetadata*   getMetadata(MusicMetadata::IdType an_id);

    StreamList *getStreams(void) { return &m_streamList; }

    void addStream(MusicMetadata *mdata);
    void removeStream(MusicMetadata *mdata);
    void updateStream(MusicMetadata *mdata);

  private:
    StreamList m_streamList;
};

//----------------------------------------------------------------------------

class AlbumArtScannerThread: public MThread
{
  public:
    explicit AlbumArtScannerThread(QStringList strList) :
            MThread("AlbumArtScanner"), m_strList(std::move(strList)) {}

    void run() override // MThread
    {
        RunProlog();
        gCoreContext->SendReceiveStringList(m_strList);
        RunEpilog();
    }

    QStringList getResult(void) { return m_strList; }

  private:
    QStringList m_strList;
};

class META_PUBLIC AlbumArtImages
{
    Q_DECLARE_TR_FUNCTIONS(AlbumArtImages);

  public:
    explicit AlbumArtImages(MusicMetadata *metadata, bool loadFromDB = true);
    explicit AlbumArtImages(MusicMetadata *metadata, const AlbumArtImages &other);
    ~AlbumArtImages();

    void           scanForImages(void);
    void           addImage(const AlbumArtImage * newImage);
    uint           getImageCount() { return m_imageList.size(); }
    AlbumArtImage *getImage(ImageType type);
    AlbumArtImage *getImageByID(int imageID);
    QStringList    getImageFilenames(void) const;
    AlbumArtList  *getImageList(void) { return &m_imageList; }
    AlbumArtImage *getImageAt(uint index);

    void dumpToDatabase(void);

    static ImageType guessImageType(const QString &filename);
    static QString   getTypeName(ImageType type);
    static QString   getTypeFilename(ImageType type);
    static ImageType getImageTypeFromName(const QString &name);

  private:
    void findImages(void);

    MusicMetadata *m_parent {nullptr};
    AlbumArtList   m_imageList;
};

Q_DECLARE_METATYPE(AlbumArtImage*);

#endif
