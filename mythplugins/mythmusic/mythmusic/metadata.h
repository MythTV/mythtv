#ifndef METADATA_H_
#define METADATA_H_

#include <qstring.h>
#include <qstringlist.h>
#include <qptrlist.h>
#include <qmap.h>
#include <qthread.h>

#include "treecheckitem.h"
#include <mythtv/uitypes.h>

class QSqlDatabase;
class AllMusic;

class Metadata
{
  public:
    Metadata(QString lfilename = "", QString lartist = "", QString lalbum = "", 
             QString ltitle = "", QString lgenre = "", int lyear = 0, 
             int ltracknum = 0, int llength = 0, int lid = 0,
             int lrating = 0, int lplaycount = 0, QString llastplay = "")
            {
                filename = lfilename;
                artist = lartist;
                album = lalbum;
                title = ltitle;
                genre = lgenre;
                year = lyear;
                tracknum = ltracknum;
                length = llength;
                id = lid;
                rating = lrating;
                playcount = lplaycount;
                lastplay = llastplay;
                changed = false;
            }

    Metadata(const Metadata &other) 
            {
                filename = other.filename;
                artist = other.artist;
                album = other.album;
                title = other.title;
                genre = other.genre;
                year = other.year;
                tracknum = other.tracknum;
                length = other.length;
                id = other.id;
                rating = other.rating;
                lastplay = other.lastplay;
                playcount = other.playcount;
                changed = false;
            }

    Metadata& operator=(Metadata *rhs);

    QString Artist() { return artist; }
    void setArtist(const QString &lartist) { artist = lartist; }
    
    QString Album() { return album; }
    void setAlbum(const QString &lalbum) { album = lalbum; }

    QString Title() { return title; }
    void setTitle(const QString &ltitle) { title = ltitle; }

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
    QString Lastplay(){return lastplay;}
    void setLastPlay();

    int PlayCount() { return playcount; }
    void incPlayCount();

    bool isInDatabase(QSqlDatabase *db);
    void dumpToDatabase(QSqlDatabase *db);

    void setField(const QString &field, const QString &data);
    void getField(const QString &field, QString *data);
    void getField(const QStringList& tree_levels, QString *data, const QString &paths, const QString &startdir, uint depth);
    bool areYouFinished(uint depth, uint treedepth, const QString& paths, const QString& startdir);
    void fillData(QSqlDatabase *db);
    void fillDataFromID(QSqlDatabase *db);
    void persist(QSqlDatabase *db);
    bool hasChanged(){return changed;}

  private:
    QString artist;
    QString album;
    QString title;
    QString genre;
    int year;
    int tracknum;
    int length;
    int rating;
    QString lastplay;
    int playcount;

    unsigned int id;
    QString filename;
    bool    changed;
};

bool operator==(const Metadata& a, const Metadata& b);
bool operator!=(const Metadata& a, const Metadata& b);

class MusicNode
{
    //  Not a root of the music tree, and
    //  not a leaf, but anything in the
    //  middle
    
  public:
  
    MusicNode(QString a_title, const QString& a_startdir, const QString& a_paths, QStringList tree_levels, uint depth);

    void        insert(Metadata* inserter);
    QString     getTitle(){return my_title;}
    MusicNode*  findRightNode(QStringList tree_levels, Metadata *inserter, uint depth);
    void        printYourself(int indent_amount);   // debugging
    void        clearTracks(){my_tracks.clear();}
    void        putYourselfOnTheListView(TreeCheckItem *parent, bool show_node);
    void        writeTree(GenericTree *tree_to_write_to, int a_counter);
    void        sort();
    
  private:
  
    QPtrList<Metadata>  my_tracks;
    QPtrList<MusicNode> my_subnodes;
    QString             my_title;
    QString             my_level;
    QString             startdir;
    QString             paths;
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
  
    AllMusic(QSqlDatabase *ldb, QString path_assignment, QString a_startdir);
    ~AllMusic();

    QString     getLabel(int an_id, bool *error_flag);
    Metadata*   getMetadata(int an_id);
    void        save();
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
    void        putYourselfOnTheListView(TreeCheckItem *where);
    void        putCDOnTheListView(CDCheckItem *where);
    bool        doneLoading(){return done_loading;}
    bool        cleanOutThreads();
    
  private:
  
    QPtrList<Metadata>  all_music;
    QPtrList<MusicNode> top_nodes;
    MusicNode           *root_node;
    
    
    QSqlDatabase       *db;


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


};


#endif
