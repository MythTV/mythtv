#ifndef METADATA_H_
#define METADATA_H_

#include <qstring.h>

class QSqlDatabase;

class Metadata
{
  public:
    Metadata(QString lfilename = "", QString lartist = "", QString lalbum = "", 
             QString ltitle = "", QString lgenre = "", int lyear = 0, 
             int ltracknum = 0, int llength = 0, int lid = 0)
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
            }

   ~Metadata() {}

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

    unsigned int ID() { return id; }
    void setID(int lid) { id = lid; }

    QString Filename() const { return filename; }
    void setFilename(QString &lfilename) { filename = lfilename; }


    void setField(QString field, QString data);
    void fillData(QSqlDatabase *db);
    void fillDataFromID(QSqlDatabase *db);

  private:
    QString artist;
    QString album;
    QString title;
    QString genre;
    int year;
    int tracknum;
    int length;

    unsigned int id;
    
    QString filename;
};

bool operator==(const Metadata& a, const Metadata& b);
bool operator!=(const Metadata& a, const Metadata& b);

#endif
