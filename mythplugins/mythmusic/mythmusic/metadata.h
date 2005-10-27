#ifndef METADATA_H_
#define METADATA_H_

#include <qstring.h>
#include <qstringlist.h>
#include <qptrlist.h>
#include <qmap.h>
#include <qthread.h>

#include "treecheckitem.h"
#include <mythtv/uitypes.h>

class AllMusic;

class Metadata
{
  public:
    Metadata(QString lfilename = "", QString lartist = "", QString lcompilation_artist = "", 
             QString lalbum = "", QString ltitle = "", QString lgenre = "",
             int lyear = 0, int ltracknum = 0, int llength = 0, int lid = 0,
             int lrating = 0, int lplaycount = 0, QString llastplay = "",
             bool lcompilation = false)
            {
                filename = lfilename;
                artist = lartist;
                compilation_artist = lcompilation_artist;
                album = lalbum;
                title = ltitle;
                formattedartist = "";
                formattedtitle = "";
                 genre = lgenre;
                year = lyear;
                tracknum = ltracknum;
                length = llength;
                id = lid;
                rating = lrating;
                playcount = lplaycount;
                lastplay = llastplay;
                compilation = lcompilation;
                changed = false;
                show = true;
            }

    Metadata(const Metadata &other) 
            {
                filename = other.filename;
                artist = other.artist;
                compilation_artist = other.compilation_artist;
                album = other.album;
                title = other.title;
                formattedartist = other.formattedartist;
                formattedtitle = other.formattedtitle;
                genre = other.genre;
                year = other.year;
                tracknum = other.tracknum;
                length = other.length;
                id = other.id;
                rating = other.rating;
                lastplay = other.lastplay;
                playcount = other.playcount;
                compilation = other.compilation;
                show = other.show;
                changed = false;
            }

    Metadata& operator=(Metadata *rhs);

    QString Artist() { return artist; }
    void setArtist(const QString &lartist) { artist = lartist; formattedartist = formattedtitle = ""; }
    
    QString CompilationArtist() { return compilation_artist; }
    void setCompilationArtist(const QString &lcompilation_artist) { compilation_artist = lcompilation_artist; formattedartist = formattedtitle = ""; }
    
    QString Album() { return album; }
    void setAlbum(const QString &lalbum) { album = lalbum; formattedartist = formattedtitle = ""; }

    QString Title() { return title; }
    void setTitle(const QString &ltitle) { title = ltitle; }

    QString FormatArtist();
    QString FormatTitle();

    QString Genre() { return genre; }
    void setGenre(const QString &lgenre) { genre = lgenre; }

    int Year() { return year; }
    void setYear(int lyear) { year = lyear; }
 
    int Track() { return tracknum; }
    void setTrack(int ltrack) { tracknum = ltrack; }

    int Length() { return length; }
    void setLength(int llength) { length = llength; }

    int Playcount() { return playcount; }
    void setPlaycount(int lplaycount) { playcount = lplaycount; }

    unsigned int ID() { return id; }
    void setID(int lid) { id = lid; }
    
    QString Filename() const { return filename; }
    void setFilename(QString &lfilename) { filename = lfilename; }
    
    int Rating() { return rating; }
    void decRating();
    void incRating();

    double LastPlay();
    QString LastPlayStr() { return lastplay; }
    void setLastPlay();

    int PlayCount() { return playcount; }
    void incPlayCount();

    bool isVisible() { return show; }
    void setVisible(bool visible) { show = visible; }

    // track is part of a compilation album
    bool Compilation() { return compilation; }
    void setCompilation(bool state) { compilation = state; formattedartist = formattedtitle = ""; }
    bool determineIfCompilation(bool cd = false);
    
    bool isInDatabase(QString startdir);
    void dumpToDatabase(QString startdir);
    void updateDatabase(QString startdir);
    void setField(const QString &field, const QString &data);
    void getField(const QString &field, QString *data);
    void getField(const QStringList& tree_levels, QString *data, const QString &paths, const QString &startdir, uint depth);
    bool areYouFinished(uint depth, uint treedepth, const QString& paths, const QString& startdir);
    void fillData();
    void fillDataFromID();
    void persist();
    bool hasChanged(){return changed;}
    static void setArtistAndTrackFormats();

    static void SetStartdir(const QString &dir);

  private:
    void setCompilationFormatting(bool cd = false);
    QString formatReplaceSymbols(const QString &format);

    QString artist;
    QString compilation_artist;
    QString album;
    QString title;
    QString formattedartist;
    QString formattedtitle;
    QString genre;
    int year;
    int tracknum;
    int length;
    int rating;
    QString lastplay;
    int playcount;
    bool compilation;
     
    unsigned int id;
    QString filename;
    bool    changed;

    bool    show;

    static QString m_startdir;

    // Various formatting strings
    static QString formatnormalfileartist;
    static QString formatnormalfiletrack;
    static QString formatnormalcdartist;
    static QString formatnormalcdtrack;

    static QString formatcompilationfileartist;
    static QString formatcompilationfiletrack;
    static QString formatcompilationcdartist;
    static QString formatcompilationcdtrack;
};

bool operator==(const Metadata& a, const Metadata& b);
bool operator!=(const Metadata& a, const Metadata& b);

class MetadataPtrList : public QPtrList<Metadata>
{
  public:
    MetadataPtrList() {}
    ~MetadataPtrList() {}

  protected:
    int compareItems(QPtrCollection::Item item1,
                     QPtrCollection::Item item2);
};

class MusicNode;
class MusicNodePtrList : public QPtrList<MusicNode>
{
  public:
    MusicNodePtrList() {}
    ~MusicNodePtrList() {}

  protected:
    int compareItems(QPtrCollection::Item item1,
                     QPtrCollection::Item item2);
};

class MusicNode
{
    //  Not a root of the music tree, and
    //  not a leaf, but anything in the
    //  middle
    
  public:
  
    MusicNode(QString a_title, QStringList tree_levels, uint depth);
   ~MusicNode();

    void        insert(Metadata* inserter);
    QString     getTitle(){return my_title;}
    MusicNode*  findRightNode(QStringList tree_levels, Metadata *inserter, 
                uint depth);
    void        printYourself(int indent_amount);   // debugging
    void        clearTracks() { my_tracks.clear(); }
    void        putYourselfOnTheListView(TreeCheckItem *parent, bool show_node);
    void        writeTree(GenericTree *tree_to_write_to, int a_counter);
    void        sort();
    void        setPlayCountMin(int tmp_min) { playcountMin = tmp_min; }
    void        setPlayCountMax(int tmp_max) { playcountMax = tmp_max; }
    void        setLastPlayMin(double tmp_min) { lastplayMin = tmp_min; }
    void        setLastPlayMax(double tmp_max) { lastplayMax = tmp_max; }

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

    int                 playcountMin;
    int                 playcountMax;
    double              lastplayMin;
    double              lastplayMax;
};

class MetadataLoadingThread : public QThread
{

  public:

    MetadataLoadingThread(AllMusic *parent_ptr);
    virtual void run();
    
  private:
  
    AllMusic *parent;
};

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
    void        save();
    bool        startLoading(void);
    void        resync();   //  After a CD rip, for example
    void        clearCDData();
    void        addCDTrack(Metadata *the_track);
    bool        checkCDTrack(Metadata *the_track);
    bool        getCDMetadata(int the_track, Metadata *some_metadata);
    QString     getCDTitle(){return cd_title;}
    void        setCDTitle(const QString &a_title){cd_title = a_title;}
    void        buildTree();
    void        printTree();    // debugging
    void        sortTree();
    void        writeTree(GenericTree *tree_to_write_to);
    void        intoTree(Metadata* inserter);
    MusicNode*  findRightNode(Metadata* inserter, uint depth);
    void        setSorting(QString a_paths);
    bool        putYourselfOnTheListView(TreeCheckItem *where, int how_many);
    void        putCDOnTheListView(CDCheckItem *where);
    bool        doneLoading(){return done_loading;}
    bool        cleanOutThreads();
    int         getCDTrackCount(){return cd_data.count();}
    void        resetListings(){last_listed = -1;}
    void        setAllVisible(bool visible);
    
  private:
  
    MetadataPtrList     all_music;
    MusicNodePtrList    top_nodes;
    MusicNode           *root_node;
    
    

    //  NB: While a QMap is VALUE BASED the
    //  values we are copying here are pointers,
    //  so they still reference the correct data
    //  If, however, you create or delete metadata
    //  you NEED to clear and rebuild the map
    typedef QMap<int, Metadata*> MusicMap;
    MusicMap music_map;
    
    typedef QValueList<Metadata>  ValueMetadata;
    ValueMetadata                 cd_data; //  More than one cd player?
    QString                       cd_title;

    QString     startdir;
    QString     paths;
    QStringList tree_levels;
    
    
    MetadataLoadingThread   *metadata_loader;
    bool                     done_loading;
    int                      last_listed;

    int                      playcountMin;
    int                      playcountMax;
    double                   lastplayMin;
    double                   lastplayMax;

};


#endif
