#ifndef METADATA_H_
#define METADATA_H_

// C++
#include <vector>
using namespace std;

// qt
#include <QStringList>
#include <QMap>

// mythtv
#include <uitypes.h>
#include <mthread.h>

// mythmusic
#include "treecheckitem.h"


class AllMusic;
class AlbumArtImages;
class PlaylistContainer;
class MetaIO;

enum ImageType
{
    IT_UNKNOWN = 0,
    IT_FRONTCOVER,
    IT_BACKCOVER,
    IT_CD,
    IT_INLAY,
    IT_LAST
};

class AlbumArtImage
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

class Metadata
{
  public:

    typedef uint32_t IdType;

    Metadata(QString lfilename = "", QString lartist = "", QString lcompilation_artist = "",
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
                   m_length(llength),
                   m_rating(lrating),
                   m_directoryid(-1),
                   m_artistid(-1),
                   m_compartistid(-1),
                   m_albumid(-1),
                   m_genreid(-1),
                   m_lastplay(llastplay),
                   m_dateadded(ldateadded),
                   m_playcount(lplaycount),
                   m_compilation(lcompilation),
                   m_albumArt(NULL),
                   m_id(lid),
                   m_filename(lfilename),
                   m_changed(false),
                   m_show(true)
    {
        checkEmptyFields();
    }

    ~Metadata();

    Metadata(const Metadata &other)
    {
        *this = other;
         m_changed = false;
    }

    Metadata& operator=(const Metadata &other);

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

    int Length() const { return m_length; }
    void setLength(int llength) { m_length = llength; }

    int Playcount() const { return m_playcount; }
    void setPlaycount(int lplaycount) { m_playcount = lplaycount; }

    IdType ID() const { return m_id; }
    void setID(IdType lid) { m_id = lid; }
    void setRepo(RepoType repo) { m_id = (m_id & METADATA_ID_MASK) | (repo << METADATA_REPO_SHIFT); }

    bool isCDTrack(void) { return ID_TO_REPO(m_id) == RT_CD; }

    QString Filename() const { return m_filename; }
    void setFilename(const QString &lfilename) { m_filename = lfilename; }

    QString Format() const { return m_format; }
    void setFormat(const QString &lformat) { m_format = lformat; }

    int Rating() const { return m_rating; }
    void decRating();
    void incRating();
    void setRating(int lrating) { m_rating = lrating; }

    QDateTime LastPlay() const { return m_lastplay; }
    void setLastPlay();

    int PlayCount() const { return m_playcount; }
    void incPlayCount();

    bool isVisible() const { return m_show; }
    void setVisible(bool visible) { m_show = visible; }

    // track is part of a compilation album
    bool Compilation() const { return m_compilation; }
    void setCompilation(bool state)
    {
        m_compilation = state;
        m_formattedartist.clear();
        m_formattedtitle.clear();
    }
    bool determineIfCompilation(bool cd = false);

    void setEmbeddedAlbumArt(AlbumArtList &albumart);

    bool isInDatabase(void);
    void dumpToDatabase(void);
    void setField(const QString &field, const QString &data);
    void getField(const QString& field, QString *data);
    void toMap(MetadataMap &metadataMap);

    void persist(void) const;
    void UpdateModTime(void) const;
    bool hasChanged() const { return m_changed; }
    int compare(const Metadata *other) const;
    static void setArtistAndTrackFormats();

    static void SetStartdir(const QString &dir);
    static QString GetStartdir() { return m_startdir; }

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
    int m_length;
    int m_rating;
    int m_directoryid;
    int m_artistid;
    int m_compartistid;
    int m_albumid;
    int m_genreid;
    QDateTime m_lastplay;
    QDateTime m_dateadded;
    int  m_playcount;
    bool m_compilation;

    AlbumArtImages *m_albumArt;

    IdType   m_id;
    QString  m_filename;
    bool     m_changed;

    bool     m_show;

    static QString m_startdir;

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

bool operator==(const Metadata& a, const Metadata& b);
bool operator!=(const Metadata& a, const Metadata& b);

Q_DECLARE_METATYPE(Metadata *)

typedef QList<Metadata*> MetadataPtrList;
class MusicNode;
typedef QList<MusicNode*> MusicNodePtrList;

class MusicNode
{
  public:

    MusicNode(const QString &a_title, const QString &tree_level);
   ~MusicNode();

    QString     getTitle(void) const { return my_title; }
    void        putYourselfOnTheListView(TreeCheckItem *parent, bool show_node);
    void        writeTree(GenericTree *tree_to_write_to, int a_counter);
    void        sort();
    void        setPlayCountMin(int tmp_min) { m_playcountMin = tmp_min; }
    void        setPlayCountMax(int tmp_max) { m_playcountMax = tmp_max; }
    void        setLastPlayMin(double tmp_min) { m_lastplayMin = tmp_min; }
    void        setLastPlayMax(double tmp_max) { m_lastplayMax = tmp_max; }

    inline void addChild(MusicNode *child) { my_subnodes.append(child); }
    inline void addLeaf(Metadata *leaf) { my_tracks.append(leaf); }
    inline void setLeaves(MetadataPtrList leaves) { my_tracks = leaves; }

    void clear(void) 
    {
        while (!my_subnodes.isEmpty())
            delete my_subnodes.takeFirst();

        // done delete the metadata since AllMusic owns it
        my_tracks.clear();
    }

    static void SetStaticData(const QString &startdir, const QString &paths);

  private:

    MetadataPtrList     my_tracks;
    MusicNodePtrList    my_subnodes;
    QString             my_title;
    QString             my_level;

    static QString      m_startdir;
    static QString      m_paths;
    static int          m_RatingWeight;
    static int          m_PlayCountWeight;
    static int          m_LastPlayWeight;
    static int          m_RandomWeight;

    int                 m_playcountMin;
    int                 m_playcountMax;
    double              m_lastplayMin;
    double              m_lastplayMax;
};

//---------------------------------------------------------------------------

class MetadataLoadingThread : public MThread
{

  public:

    MetadataLoadingThread(AllMusic *parent_ptr);
    virtual void run();

  private:

    AllMusic *parent;
};

//---------------------------------------------------------------------------

class AllMusic
{
    //  One class to read the data once at mythmusic start
    //  And save any changes at mythmusic stop

  public:

    AllMusic(QString path_assignment, QString a_startdir);
    ~AllMusic();

    QString     getLabel(int an_id, bool *error_flag);
    Metadata*   getMetadata(int an_id);
    bool        updateMetadata(int an_id, Metadata *the_track);
    int         count() const { return m_numPcs; }
    int         countLoaded() const { return m_numLoaded; }
    void        save();
    bool        startLoading(void);
    void        resync();   //  After a CD rip, for example
    void        clearCDData();
    void        addCDTrack(Metadata *the_track);
    bool        checkCDTrack(Metadata *the_track);
    bool        getCDMetadata(int m_the_track, Metadata *some_metadata);
    QString     getCDTitle(){return m_cd_title;}
    void        setCDTitle(const QString &a_title){m_cd_title = a_title;}
    void        buildTree();
    void        sortTree();
    inline void clearTree() { m_root_node-> clear(); }
    void        writeTree(GenericTree *tree_to_write_to);
    void        setSorting(QString a_paths);
    bool        putYourselfOnTheListView(TreeCheckItem *where);
    void        putCDOnTheListView(CDCheckItem *where);
    bool        doneLoading(){return m_done_loading;}
    bool        cleanOutThreads();
    int         getCDTrackCount(){return m_cd_data.count();}
    void        resetListings(){m_last_listed = -1;}
    void        setAllVisible(bool visible);

    MetadataPtrList *getAllMetadata(void) { return &m_all_music; }

  private:

    MetadataPtrList     m_all_music;
    MusicNode           *m_root_node;

    int m_numPcs;
    int m_numLoaded;

    //  NB: While a QMap is VALUE BASED the
    //  values we are copying here are pointers,
    //  so they still reference the correct data
    //  If, however, you create or delete metadata
    //  you NEED to clear and rebuild the map
    typedef QMap<int, Metadata*> MusicMap;
    MusicMap music_map;

    typedef QList<Metadata>       ValueMetadata;
    ValueMetadata                 m_cd_data; //  More than one cd player?
    QString                       m_cd_title;

    QString     m_startdir;
    QString     m_paths;


    MetadataLoadingThread   *m_metadata_loader;
    bool                     m_done_loading;
    int                      m_last_listed;

    int                      m_playcountMin;
    int                      m_playcountMax;
    double                   m_lastplayMin;
    double                   m_lastplayMax;

};

//----------------------------------------------------------------------------

class MusicData : public QObject
{
  Q_OBJECT

  public:

    MusicData();
    ~MusicData();

  public slots:
    void reloadMusic(void);

  public:
    QString             paths;
    QString             startdir;
    PlaylistContainer  *all_playlists;
    AllMusic           *all_music;
    bool                initialized;
};

// This global variable contains the MusicData instance for the application
extern MPUBLIC MusicData *gMusicData;

//----------------------------------------------------------------------------


class AlbumArtImages
{
  public:
    AlbumArtImages(Metadata *metadata);
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

    Metadata     *m_parent;
    AlbumArtList  m_imageList;
};

Q_DECLARE_METATYPE(AlbumArtImage*);

#endif
