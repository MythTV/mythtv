#ifndef MUSICMETADATA_H_
#define MUSICMETADATA_H_

// C/C++
#include <iostream>
#include <stdint.h>

using namespace std;

// qt
#include <QStringList>
#include <QMap>
#include <QDateTime>
#include <QImage>
#include <QMetaType>
#include <QCoreApplication>

// mythtv
#include "mythmetaexp.h"
#include <mthread.h>

class AllMusic;
class AlbumArtImages;
class MetaIO;

enum ImageType
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
            id(0), filename(""), imageType(IT_UNKNOWN),
            description(""), embedded(false) {}
    AlbumArtImage(AlbumArtImage *image) :
            id(image->id), filename(image->filename), imageType(image->imageType),
            description(image->description), embedded(image->embedded) {}
     int       id;
     QString   filename;
     ImageType imageType;
     QString   description;
     bool      embedded;
};

typedef QList<AlbumArtImage*> AlbumArtList;
typedef QHash<QString,QString> MetadataMap;


enum RepoType
{
    RT_Database = 0,
    RT_CD       = 1,
    RT_Radio    = 2
};

#define METADATA_BITS_FOR_REPO 8
#define METADATA_REPO_SHIFT 24
#define METADATA_REPO_MASK 0xff000000
#define METADATA_ID_MASK 0x00ffffff

#define ID_TO_ID(x) x & METADATA_ID_MASK;
#define ID_TO_REPO(x)  x >> METADATA_REPO_SHIFT

class META_PUBLIC MusicMetadata
{
    Q_DECLARE_TR_FUNCTIONS(MusicMetadata)

  public:

    typedef uint32_t IdType;

    MusicMetadata(QString lfilename = "", QString lartist = "", QString lcompilation_artist = "",
             QString lalbum = "", QString ltitle = "", QString lgenre = "",
             int lyear = 0, int ltracknum = 0, int llength = 0, int lid = 0,
             int lrating = 0, int lplaycount = 0, QDateTime llastplay = QDateTime(),
             QDateTime ldateadded = QDateTime(), bool lcompilation = false, QString lformat = "")
                : m_artist(lartist),
                   m_compilation_artist(lcompilation_artist),
                   m_album(lalbum),
                   m_title(ltitle),
                   m_formattedartist(""),
                   m_formattedtitle(""),
                   m_genre(lgenre),
                   m_format(lformat),
                   m_year(lyear),
                   m_tracknum(ltracknum),
                   m_trackCount(0),
                   m_length(llength),
                   m_rating(lrating),
                   m_directoryid(-1),
                   m_artistid(-1),
                   m_compartistid(-1),
                   m_albumid(-1),
                   m_genreid(-1),
                   m_lastplay(llastplay),
                   m_templastplay(QDateTime()),
                   m_dateadded(ldateadded),
                   m_playcount(lplaycount),
                   m_tempplaycount(0),
                   m_compilation(lcompilation),
                   m_albumArt(NULL),
                   m_id(lid),
                   m_filename(lfilename),
                   m_fileSize(0),
                   m_changed(false),
                   m_station(""),
                   m_channel(""),
                   m_logoUrl(""),
                   m_metaFormat("")

    {
        checkEmptyFields();
    }

    MusicMetadata(int lid, QString lstation, QString lchannel, QString lurl, QString llogourl,
             QString lgenre, QString lmetaformat, QString lformat);

    ~MusicMetadata();

    MusicMetadata(const MusicMetadata &other)
    {
        *this = other;
         m_changed = false;
    }

    MusicMetadata& operator=(const MusicMetadata &other);

    QString Artist() const { return m_artist; }
    void setArtist(const QString &lartist)
    {
        m_artist = lartist; m_formattedartist.clear(); m_formattedtitle.clear();
    }

    QString CompilationArtist() const { return m_compilation_artist; }
    void setCompilationArtist(const QString &lcompilation_artist)
    {
        m_compilation_artist = lcompilation_artist;
        m_formattedartist.clear(); m_formattedtitle.clear();
    }

    QString Album() const { return m_album; }
    void setAlbum(const QString &lalbum)
    {
        m_album = lalbum; m_formattedartist.clear(); m_formattedtitle.clear();
    }

    QString Title() const { return m_title; }
    void setTitle(const QString &ltitle) { m_title = ltitle; }

    QString FormatArtist();
    QString FormatTitle();

    QString Genre() const { return m_genre; }
    void setGenre(const QString &lgenre) { m_genre = lgenre; }

    void setDirectoryId(int ldirectoryid) { m_directoryid = ldirectoryid; }
    int getDirectoryId() const { return m_directoryid; }

    void setArtistId(int lartistid) { m_artistid = lartistid; }
    int getArtistId() const { return m_artistid; }

    void setAlbumId(int lalbumid) { m_albumid = lalbumid; }
    int getAlbumId() const { return m_albumid; }

    void setGenreId(int lgenreid) { m_genreid = lgenreid; }
    int getGenreId() const { return m_genreid; }

    int Year() const { return m_year; }
    void setYear(int lyear) { m_year = lyear; }

    int Track() const { return m_tracknum; }
    void setTrack(int ltrack) { m_tracknum = ltrack; }

    int GetTrackCount() const { return m_trackCount; }
    void setTrackCount(int ltrackcount) { m_trackCount = ltrackcount; }

    int Length() const { return m_length; }
    void setLength(int llength) { m_length = llength; }

    int Playcount() const { return m_playcount; }
    void setPlaycount(int lplaycount) { m_playcount = lplaycount; }

    IdType ID() const { return m_id; }
    void setID(IdType lid) { m_id = lid; }
    void setRepo(RepoType repo) { m_id = (m_id & METADATA_ID_MASK) | (repo << METADATA_REPO_SHIFT); }

    bool isCDTrack(void) const { return ID_TO_REPO(m_id) == RT_CD; }
    bool isDBTrack(void) const { return ID_TO_REPO(m_id) == RT_Database; }
    bool isRadio(void) const { return ID_TO_REPO(m_id) == RT_Radio; }

    QString Filename(bool find = true) const;
    void setFilename(const QString &lfilename) { m_filename = lfilename; }

    uint64_t FileSize() const;
    void setFileSize(uint64_t lfilesize) { m_fileSize = lfilesize; }

    QString Format() const { return m_format; }
    void setFormat(const QString &lformat) { m_format = lformat; }

    int Rating() const { return m_rating; }
    void decRating();
    void incRating();
    void setRating(int lrating) { m_rating = lrating; }

    QDateTime LastPlay() const { return m_lastplay; }
    void setLastPlay();
    void setLastPlay(QDateTime lastPlay);

    int PlayCount() const { return m_playcount; }
    void incPlayCount();

    // track is part of a compilation album
    bool Compilation() const { return m_compilation; }
    void setCompilation(bool state)
    {
        m_compilation = state;
        m_formattedartist.clear();
        m_formattedtitle.clear();
    }
    bool determineIfCompilation(bool cd = false);

    void setStation(const QString &station) { m_station = station; }
    QString Station(void) { return m_station; }

    void setChannel(const QString &channel) { m_channel = channel; }
    QString Channel(void) { return m_channel; }

    void setUrl(const QString &url) { m_filename = url; }
    QString Url(void) { return m_filename; }

    void setLogoUrl(const QString &logourl) { m_logoUrl = logourl; }
    QString LogoUrl(void) { return m_logoUrl; }

    void setMetadataFormat(const QString &metaformat) { m_metaFormat = metaformat; }
    QString MetadataFormat(void) { return m_metaFormat; }

    void setEmbeddedAlbumArt(AlbumArtList &albumart);

    void reloadMetadata(void);
    void dumpToDatabase(void);
    void setField(const QString &field, const QString &data);
    void getField(const QString& field, QString *data);
    void toMap(MetadataMap &metadataMap, const QString &prefix = "");

    void persist(void);
    void UpdateModTime(void) const;
    bool hasChanged() const { return m_changed; }
    int  compare(const MusicMetadata *other) const;

    // static functions
    static MusicMetadata *createFromFilename(const QString &filename);
    static MusicMetadata *createFromID(int trackid);
    static void setArtistAndTrackFormats();
    static QStringList fillFieldList(QString field);

    // this looks for any image available - preferring a front cover if available
    QString getAlbumArtFile(void);
    // this looks only for the given image type
    QString getAlbumArtFile(ImageType type);

    AlbumArtImages *getAlbumArtImages(void);
    void reloadAlbumArtImages(void);

    MetaIO *getTagger(void);

  private:
    void setCompilationFormatting(bool cd = false);
    QString formatReplaceSymbols(const QString &format);
    void checkEmptyFields(void);

    QString m_artist;
    QString m_compilation_artist;
    QString m_album;
    QString m_title;
    QString m_formattedartist;
    QString m_formattedtitle;
    QString m_genre;
    QString m_format;
    int m_year;
    int m_tracknum;
    int m_trackCount;
    int m_length;
    int m_rating;
    int m_directoryid;
    int m_artistid;
    int m_compartistid;
    int m_albumid;
    int m_genreid;
    QDateTime m_lastplay;
    QDateTime m_templastplay;
    QDateTime m_dateadded;
    int  m_playcount;
    int  m_tempplaycount;
    bool m_compilation;

    AlbumArtImages *m_albumArt;

    IdType   m_id;
    QString  m_filename;
    uint64_t m_fileSize;
    bool     m_changed;

    // radio stream stuff
    QString m_station;
    QString m_channel;
    QString m_logoUrl;
    QString m_metaFormat;

    // Various formatting strings
    static QString m_formatnormalfileartist;
    static QString m_formatnormalfiletrack;
    static QString m_formatnormalcdartist;
    static QString m_formatnormalcdtrack;

    static QString m_formatcompilationfileartist;
    static QString m_formatcompilationfiletrack;
    static QString m_formatcompilationcdartist;
    static QString m_formatcompilationcdtrack;
};

bool operator==(const MusicMetadata& a, const MusicMetadata& b);
bool operator!=(const MusicMetadata& a, const MusicMetadata& b);

Q_DECLARE_METATYPE(MusicMetadata *)

typedef QList<MusicMetadata*> MetadataPtrList;
Q_DECLARE_METATYPE(MetadataPtrList *)

//---------------------------------------------------------------------------

class META_PUBLIC MetadataLoadingThread : public MThread
{

  public:

    MetadataLoadingThread(AllMusic *parent_ptr);
    virtual void run();

  private:

    AllMusic *parent;
};

//---------------------------------------------------------------------------

class META_PUBLIC AllMusic
{
    Q_DECLARE_TR_FUNCTIONS(AllMusic)

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

    bool        doneLoading() const { return m_done_loading; }
    bool        cleanOutThreads();

    MetadataPtrList *getAllMetadata(void) { return &m_all_music; }
    MetadataPtrList *getAllCDMetadata(void) { return &m_cdData; }

    bool isValidID(int an_id);

  private:
    MetadataPtrList     m_all_music;

    int m_numPcs;
    int m_numLoaded;

    typedef QMap<int, MusicMetadata*> MusicMap;
    MusicMap music_map;

    // cd stuff
    MetadataPtrList m_cdData; //  More than one cd player?
    QString m_cdTitle;

    MetadataLoadingThread   *m_metadata_loader;
    bool                     m_done_loading;
    int                      m_last_listed;

    int                      m_playcountMin;
    int                      m_playcountMax;
    double                   m_lastplayMin;
    double                   m_lastplayMax;
};

typedef QList<MusicMetadata*> StreamList;

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


class META_PUBLIC AlbumArtImages
{
    Q_DECLARE_TR_FUNCTIONS(AlbumArtImages)

  public:
    AlbumArtImages(MusicMetadata *metadata);
    ~AlbumArtImages();

    void           addImage(const AlbumArtImage &newImage);
    uint           getImageCount() { return m_imageList.size(); }
    AlbumArtImage *getImage(ImageType type);
    QStringList    getImageFilenames(void) const;
    AlbumArtList  *getImageList(void) { return &m_imageList; }
    AlbumArtImage *getImageAt(uint index);

    void dumpToDatabase(void);

    static ImageType guessImageType(const QString &filename);
    static QString   getTypeName(ImageType type);
    static QString   getTypeFilename(ImageType type);

  private:
    void findImages(void);

    MusicMetadata *m_parent;
    AlbumArtList   m_imageList;
};

Q_DECLARE_METATYPE(AlbumArtImage*);

#endif
