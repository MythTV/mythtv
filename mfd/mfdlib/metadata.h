#ifndef METADATA_H_
#define METADATA_H_
/*
	metadata.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for metadata object class hierarchy

*/

#include <inttypes.h>
        
#include <qstring.h>
#include <qurl.h>
#include <qdatetime.h>
#include <qvaluelist.h>
#include <qintdict.h>
#include <qmap.h>

//
//  The number of items you can have in a collection. Note this is _not_
//  some limit in a dynamic variable or something. It just makes it easy for
//  anything that has a metadata object (or information about a metaobject)
//  to know what collection it came from (divide by this number to get the
//  collection id, the remainder is the item id within that collection)
//

#define METADATA_UNIVERSAL_ID_DIVIDER 1000000


enum MetadataType
{
    MDT_unknown = 0,
    MDT_audio,
    MDT_video
};

class Metadata
{

  public:

    Metadata(
                int l_collection_id,
                int l_id, 
                QUrl l_url = QUrl(""),
                int l_rating = 5,
                QDateTime l_last_played = QDateTime(),
                int l_play_count = 0
            );

    virtual ~Metadata();

    //
    //  Set and get
    //

    QString       getFilePath();

    QUrl          getUrl(){return url;}
    void          setUrl(QUrl l_url){url = l_url;}

    int           getRating(){return rating;}
    void          setRating(int l_rating);

    int           getId(){return id;}
    void          setId(int l_id){id = l_id;}

    int           getUniversalId(); // no set, it's a function 
                                    // of collection_id and id
                                    
    int           getDbId(){return db_id;}
    void          setDbId(int l_db_id){db_id = l_db_id;}

    int           getCollectionId(){return collection_id;}
    void          setCollectionId(int l_collection_id){collection_id = l_collection_id;}

    QDateTime     getLastPlayed(){return last_played;}
    void          setLastPlayed(QDateTime l_last_played){last_played = l_last_played;}

    int           getPlayCount(){return play_count;}
    void          setPlayCount(int l_play_count){play_count = l_play_count;}
    
    MetadataType  getType(){return metadata_type;}
    void          setMetadataType(MetadataType l_metadata_type){metadata_type = l_metadata_type;}        

    QString       getTitle() { return title; }
    void          setTitle(const QString &ltitle) { title = ltitle; }
    
    QDateTime     getDateAdded(){ return date_added;}
    void          setDateAdded(QDateTime ldate_added){ date_added = ldate_added; }
    
    QDateTime     getDateModified(){ return date_modified;}
    void          setDateModified(QDateTime ldate_modified){ date_modified = ldate_modified; }

    QString       getFormat(){ return format;}
    void          setFormat(const QString &lformat){ format = lformat;}
    
    QString       getDescription(){ return description;}
    void          setDescription(const QString &ldescription){description = ldescription;}
    
    QString       getComment(){ return comment; }
    void          setComment(const QString &lcomment){ comment = lcomment;}

    int           getSize(){ return size;}
    void          setSize(int lsize){size = lsize;}

    QString       getMythDigest(){ return myth_digest;}
    void          setMythDigest(const QString &new_digest){ myth_digest = new_digest; }
    bool          hasMythDigest(){ if(myth_digest.length() > 0){ return true ; } return false; }
    
    void          setChanged(bool y_or_n){changed = y_or_n;}
    bool          getChanged(){return changed;}

    void          markAsDuplicate(bool true_or_false){marked_as_duplicate = true_or_false;}
    bool          isDuplicate(){return marked_as_duplicate;}
    
  protected:

    int             collection_id;
    QUrl            url;
    int             rating;
    int             id;
    QDateTime       last_played;
    int             play_count;
    MetadataType    metadata_type;
    int             db_id;
    QString         title;
    QDateTime       date_added;
    QDateTime       date_modified;
    QString         format;
    QString         description;
    QString         comment;
    int             size;
    QString         myth_digest;

    bool            changed;
    bool            marked_as_duplicate;
};

class AudioMetadata : public Metadata
{
  public:

    AudioMetadata(
                    int l_collection_id,
                    int l_id,
                    QUrl l_url = QUrl(""),
                    int l_rating = 0,
                    QDateTime l_last_played = QDateTime(),
                    int l_play_count = 0,
                    QString l_artist = "", 
                    QString l_album = "", 
                    QString l_title = "", 
                    QString l_genre = "", 
                    int l_year = 0, 
                    int l_tracknum = 0, 
                    int l_length = 0
                 );
                 
    AudioMetadata(
                    QString filename,
                    QString l_artist,
                    QString l_album,
                    QString l_title,
                    QString l_genre,
                    int     l_year,
                    int     l_tracknum,
                    int     l_length
                 );
                 
    ~AudioMetadata();

    QString     getArtist() { return artist; }
    void        setArtist(const QString &lartist) { artist = lartist; }
    
    QString     getAlbum() { return album; }
    void        setAlbum(const QString &lalbum) { album = lalbum; }


    QString     getGenre() { return genre; }
    void        setGenre(const QString &lgenre) { genre = lgenre; }

    int         getYear() { return year; }
    void        setYear(int lyear) { year = lyear; }
 
    int         getTrack() { return tracknum; }
    void        setTrack(int ltrack) { tracknum = ltrack; }

    int         getLength() { return length; }
    void        setLength(int llength) { length = llength; }
    
    int         getBpm() { return bpm; }
    void        setBpm(int lbpm) { bpm = lbpm;}
    
    int         getBitrate(){ return bitrate; }
    void        setBitrate(int lbitrate){ bitrate = lbitrate;}
    
    bool        getCompilation(){ return compilation;}
    void        setCompilation(bool yah_or_nah){ compilation = yah_or_nah;}
    
    QString     getComposer(){ return composer; }
    void        setComposer(const QString lcomposer){composer = lcomposer;}

    int         getDiscCount(){ return disc_count;}
    void        setDiscCount(int ldisc_count){ disc_count = ldisc_count;}
    
    int         getDiscNumber(){ return disc_number;}
    void        setDiscNumber(int ldisc_number){ disc_number = ldisc_number;}
    
    QString     getEqPreset(){ return eq_preset;}
    void        setEqPreset(const QString &leq_preset){ eq_preset = leq_preset;}
    
    int         getRelativeVolume(){ return relative_volume;}
    void        setRelativeVolume(int lrelative_volume){relative_volume = lrelative_volume;}
    
    int         getSampleRate(){ return sample_rate;}
    void        setSampleRate(int lsample_rate){ sample_rate = lsample_rate;}
    
    int         getStartTime(){ return start_time;}
    void        setStartTime(int lstart_time){ start_time = lstart_time;}

    int         getStopTime(){ return stop_time;}
    void        setStopTime(int lstop_time){ stop_time = lstop_time;}

    int         getTrackCount(){ return track_count;}
    void        setTrackCount(int ltrack_count){track_count = ltrack_count;}

  protected:

    QString     artist;
    QString     album;
    QString     genre;
    int         year;
    int         tracknum;
    int         length;
    int         bpm;
    int         bitrate;
    bool        compilation;
    QString     composer;

    int         disc_count;
    int         disc_number;
    QString     eq_preset;

    int         relative_volume;
    int         sample_rate;
    int         start_time;
    int         stop_time;
    int         track_count;
};


class Playlist
{
  public:
  
    Playlist(int l_collection_id, QString new_name, QString raw_songlist, uint new_id);
    Playlist(int l_collection_id, QString new_name, QValueList<int> *l_song_references, uint new_id);

    virtual ~Playlist();

    uint             getId(){return id;}
    int              getDbId(){return db_id;}
    void             setId(int an_int){id = an_int;}
    void             setDbId(int an_int){db_id = an_int;}
    uint             getUniversalId();
    QString          getName(){return name;}
    void             setName(const QString &new_name){ name = new_name; }
    QString          getRawSongList(){return raw_song_list;}
    uint             getCount(){return song_references.count();}
    uint             getFlatCount(){return flat_tree_item_count;}
    QValueList<int>  getList(){return song_references;}
    QValueList<int>* getListPtr(){return &song_references;}
    QValueList<int>* getDbList(){return &db_references;}
    void             addToList(int an_id);
    bool             removeFromList(int an_id);

    void             mapDatabaseToId(
                                        QIntDict<Metadata> *the_metadata, 
                                        QValueList<int>* reference_list,
                                        QValueList<int>* song_list,
                                        QIntDict<Playlist> *the_playlists,
                                        int depth,
                                        bool flatten_playlists = true,
                                        bool prune_dead = false
                                    );


    void             mapIdToDatabase(
                                        QIntDict<Metadata> *the_metadata,
                                        QIntDict<Playlist> *the_playlists
                                    );


    uint             getCollectionId(){return collection_id;}    
    bool             internalChange(){ return internal_change; }
    void             internalChange(bool uh_huh_or_nope_not_me){internal_change = uh_huh_or_nope_not_me;}
    bool             waitingForList(){ return waiting_for_list; }
    void             waitingForList(bool uh_huh_or_nope_not_me){waiting_for_list = uh_huh_or_nope_not_me;}
    void             checkAgainstMetadata(QIntDict<Metadata> *the_metadata);
    void             addToIndirectMap(int key, int value);
    int              getFromIndirectMap(int key);
    void             removeFromIndirectMap(int key);
    QMap<int, int>*  getIndirectMap(){return &indirect_map;}
    bool             isEditable(){return is_editable;}
    void             isEditable(bool yes_or_no){is_editable = yes_or_no;}
    bool             userNewList(){ return user_new_list; }
    void             userNewList(bool y_or_n){ user_new_list = y_or_n; }
    

  private:
  
    QString          name;
    QValueList<int>  song_references;
    QValueList<int>  db_references;
    uint             id;
    int              db_id;
    uint             collection_id;
    uint             flat_tree_item_count;
    bool             internal_change;
    bool             waiting_for_list;
    bool             is_editable;
    QString          raw_song_list;

    //
    //  Stupid thing to handle one too many levels of indirection in
    //  playlists that come from iTunes
    //

    QMap<int, int>  indirect_map;
    
    //
    //  For marking lists as new from the user
    //
    
    bool            user_new_list;
};

#endif
