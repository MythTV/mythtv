#ifndef METADATA_H_
#define METADATA_H_
/*
	metadata.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for metadata object class hierarchy

*/


#include <qstring.h>
#include <qurl.h>
#include <qdatetime.h>
#include <qvaluelist.h>
#include <qintdict.h>
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
                int l_rating = 0,
                QDateTime l_last_played = QDateTime(),
                int l_play_count = 0,
                bool l_ephemeral = false
            );

    virtual ~Metadata();

    //
    //  Get 'em
    //
  
    QUrl          getUrl(){return url;}
    int           getRating(){return rating;}
    int           getId(){return id;}
    int           getCollectionId(){return collection_id;}
    QDateTime     getLastPlayed(){return last_played;}
    int           getPlayCount(){return play_count;}
    bool          getEphemeral(){return ephemeral;}
    MetadataType  getType(){return metadata_type;}
    int           getDatabaseId(){return db_id;}
    int           getUniversalId();
        
    //
    //  Set 'em
    //    
    
    void    setUrl(QUrl l_url){url = l_url;}
    void    setRating(int l_rating){rating = l_rating;}
    void    setId(int l_id){id = l_id;}
    void    setLastPlayed(QDateTime l_last_played){last_played = l_last_played;}
    void    setPlayCount(int l_play_count){play_count = l_play_count;}
    
  protected:

    int             collection_id;
    QUrl            url;
    int             rating;
    int             id;
    QDateTime       last_played;
    int             play_count;
    bool            ephemeral;
    MetadataType    metadata_type;
    int             db_id;
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
                    bool l_ephemeral = false,
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

    QString     getTitle() { return title; }
    void        setTitle(const QString &ltitle) { title = ltitle; }

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
    
    QString     getComment(){ return comment; }
    void        setComment(const QString &lcomment){ comment = lcomment;}

    bool        getCompilation(){ return compilation;}
    void        setCompilation(bool yah_or_nah){ compilation = yah_or_nah;}
    
    QString     getComposer(){ return composer; }
    void        setComposer(const QString lcomposer){composer = lcomposer;}

    QDateTime   getDateAdded(){ return date_added;}
    void        setDateAdded(QDateTime ldate_added){ date_added = ldate_added; }
    
    QDateTime   getDateModified(){ return date_modified;}
    void        setDateModified(QDateTime ldate_modified){ date_modified = ldate_modified; }

    int         getDiscCount(){ return disc_count;}
    void        setDiscCount(int ldisc_count){ disc_count = ldisc_count;}
    
    int         getDiscNumber(){ return disc_number;}
    void        setDiscNumber(int ldisc_number){ disc_number = ldisc_number;}
    
    QString     getEqPreset(){ return eq_preset;}
    void        setEqPreset(const QString &leq_preset){ eq_preset = leq_preset;}
    
    QString     getFormat(){ return format;}
    void        setFormat(const QString &lformat){ format = lformat;}
    
    QString     getDescription(){ return description;}
    void        setDescription(const QString &ldescription){description = ldescription;}
    
    int         getRelativeVolume(){ return relative_volume;}
    void        setRelativeVolume(int lrelative_volume){relative_volume = lrelative_volume;}
    
    int         getSampleRate(){ return sample_rate;}
    void        setSampleRate(int lsample_rate){ sample_rate = lsample_rate;}
    
    int         getSize(){ return size;}
    void        setSize(int lsize){size = lsize;}
    
    int         getStartTime(){ return start_time;}
    void        setStartTime(int lstart_time){ start_time = lstart_time;}

    int         getStopTime(){ return stop_time;}
    void        setStopTime(int lstop_time){ stop_time = lstop_time;}

    int         getTrackCount(){ return track_count;}
    void        setTrackCount(int ltrack_count){track_count = ltrack_count;}

  protected:

    QString     artist;
    QString     album;
    QString     title;
    QString     genre;
    int         year;
    int         tracknum;
    int         length;
    int         bpm;
    int         bitrate;
    QString     comment;
    bool        compilation;
    QString     composer;
    QDateTime   date_added;
    QDateTime   date_modified;
    int         disc_count;
    int         disc_number;
    QString     eq_preset;
    QString     format;
    QString     description;
    int         relative_volume;
    int         sample_rate;
    int         size;
    int         start_time;
    int         stop_time;
    int         track_count;
};


class Playlist
{
  public:
  
    Playlist(int l_collection_id, QString new_name, QString raw_songlist, uint new_id);
    ~Playlist();

    uint             getId(){return id;}
    uint             getUniversalId();
    QString          getName(){return name;}
    uint             getCount(){return song_references.count();}
    QValueList<uint> getList(){return song_references;}
    void             mapDatabaseToId(QIntDict<Metadata> *the_metadata);    
    uint             getCollectionId(){return collection_id;}    
    
  private:
  
    QString          name;
    QValueList<uint> song_references;
    QValueList<uint> db_references;
    uint             id;
    uint             collection_id;
};

#endif
