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


enum metadata_type
{
    MT_GENERIC = 0,
    MT_AUDIO
};

class Metadata
{

  public:

    //
    //  Get 'em
    //
  
    metadata_type getType(){return type;}
    QUrl          getUrl(){return url;}
    int           getRating(){return rating;}
    int           getId(){return id;}
    QDateTime     getLastPlayed(){return last_played;}
    int           getPlayCount(){return play_count;}
    bool          getEphemeral(){return ephemeral;}
        
    //
    //  Set 'em
    //    
    
    void    setUrl(QUrl l_url){url = l_url;}
    void    setRating(int l_rating){rating = l_rating;}
    void    setId(int l_id){id = l_id;}
    void    setLastPlayed(QDateTime l_last_played){last_played = l_last_played;}
    void    setPlayCount(int l_play_count){play_count = l_play_count;}
    
  protected:

    Metadata(
                int l_id, 
                QUrl l_url = QUrl(""),
                int l_rating = 0,
                QDateTime l_last_played = QDateTime(),
                int l_play_count = 0,
                bool l_ephemeral = false
            );

    QUrl            url;
    metadata_type   type;
    int             rating;
    int             id;
    QDateTime       last_played;
    int             play_count;
    bool            ephemeral;
};

class AudioMetadata : public Metadata
{
  public:

    AudioMetadata(  int l_id,
                    int db_id,
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


    QString getArtist() { return artist; }
    void    setArtist(const QString &lartist) { artist = lartist; }
    
    QString getAlbum() { return album; }
    void    setAlbum(const QString &lalbum) { album = lalbum; }

    QString getTitle() { return title; }
    void    setTitle(const QString &ltitle) { title = ltitle; }

    QString getGenre() { return genre; }
    void    setGenre(const QString &lgenre) { genre = lgenre; }

    int     getYear() { return year; }
    void    setYear(int lyear) { year = lyear; }
 
    int     getTrack() { return tracknum; }
    void    setTrack(int ltrack) { tracknum = ltrack; }

    int     getLength() { return length; }
    void    setLength(int llength) { length = llength; }
    
  private:

    QString artist;
    QString album;
    QString title;
    QString genre;
    int year;
    int tracknum;
    int length;

    int db_id;    
    bool changed;
};



#endif
