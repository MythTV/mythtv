#ifndef METADATA_H_
#define METADATA_H_

#include <qregexp.h>
#include <qstring.h>

#include <mythtv/mythcontext.h>
#include <qpixmap.h>
#include <qimage.h>

class Metadata
{
  public:
    Metadata(QString lfilename = "", QString lcoverfile = "", 
             QString ltitle = "", int lyear = 0, QString linetref = "", 
             QString ldirector = "", QString lplot = "", 
             float luserrating = 0.0, QString lrating = "", int llength = 0, 
             int lid = 0, int lshowlevel = 1, int lchildID = -1,
             bool lbrowse = true, QString lplaycommand = "",
             QString lcategory = "",
             QStringList lgenres = QStringList(),
             QStringList lcountries = QStringList())
    {
        coverImage = NULL;
        coverPixmap = NULL;
        filename = lfilename;
        coverfile = lcoverfile;
        title = ltitle;
        year = lyear;
        inetref = linetref;
        director = ldirector;
        plot = lplot;
        luserrating = luserrating;
        rating = lrating;
        length = llength;
        showlevel = lshowlevel;
        id = lid;
        childID = lchildID;
        browse = lbrowse;
        playcommand = lplaycommand;
        category = lcategory;
        genres = lgenres;
        countries = lcountries;
    }
    
    Metadata(const Metadata &other) 
    {
        coverImage = NULL;
        coverPixmap = NULL;
        filename = other.filename;
        coverfile = other.coverfile;
        title = other.title;
        year = other.year;
        inetref = other.inetref;
        director = other.director;
        plot = other.plot;
        userrating = other.userrating;
        rating = other.rating;
        length = other.length;
        showlevel = other.showlevel;
        id = other.id;
        childID = other.childID;
        browse = other.browse;
        playcommand = other.playcommand;
        category = other.category;
        genres = other.genres;
        countries = other.countries;
    }
    
    static void purgeByFilename( const QString& filename );
    static void purgeByID( int ID );
    
    void reset()
    {
        if (coverImage) delete coverImage;
        coverImage = NULL;
        coverPixmap = NULL;
        filename = "";
        coverfile = "";
        title = "";
        year = 0;
        inetref = "";
        director = "";
        plot = "";
        userrating = 0.0;
        rating = "";
        length = 0;
        showlevel = 1;
        id = 0;
        childID = -1;
        browse = true;
        playcommand = "";
        category = "";
        genres = QStringList();
        countries = QStringList();
        player = "";

    }
    
    ~Metadata() { if (coverImage) delete coverImage; }


    QString Title() { return title; }
    void setTitle(const QString &ltitle) { title = ltitle; }
    
    int Year() { return year; }
    void setYear(const int &lyear) { year = lyear; }

    QString InetRef() { return inetref; }
    void setInetRef(const QString &linetref) { inetref = linetref; }

    QString Director() { return director; }
    void setDirector(const QString &ldirector) { director = ldirector; }

    QString Plot() { return plot; }
    void setPlot(const QString &lplot) { plot = lplot; }

    float UserRating() { return userrating; }
    void setUserRating(float luserrating) { userrating = luserrating; }
 
    QString Rating() { return rating; }
    void setRating(QString lrating) { rating = lrating; }

    int Length() { return length; }
    void setLength(int llength) { length = llength; }

    unsigned int ID() { return id; }
    void setID(int lid) { id = lid; }

    int ChildID() { return childID; }
    void setChildID(int lchildID) { childID = lchildID; }
    
    bool Browse() {return browse; }
    void setBrowse(bool y_or_n){ browse = y_or_n;}
   
    QString PlayCommand() {return playcommand;}
    void setPlayCommand(const QString &new_command){playcommand = new_command;}
    
    int ShowLevel() { return showlevel; }
    void setShowLevel(int lshowlevel) { showlevel = lshowlevel; }

    QString Filename() const { return filename; }
    void setFilename(QString &lfilename) { filename = lfilename; }

    QString CoverFile() const { return coverfile; }
    void setCoverFile(QString &lcoverfile) { coverfile = lcoverfile; }
    
    QString Player() const { return player; }
    void setPlayer(const QString &_player) { player = _player; }

    QString Category() const { return category;}
    void setCategory(QString lcategory){category = lcategory;}
    QStringList Genres() const { return genres; }
    void setGenres(QStringList lgenres){genres = lgenres;}

    QStringList Countries() const { return countries;}
    void setCountries(QStringList lcountries){countries = lcountries;}

    void guessTitle();
    void eatBraces(const QString &left_brace, const QString &right_brace);
    void setField(QString field, QString data);
    void dumpToDatabase();
    void updateDatabase();
    bool fillDataFromID();
    bool fillDataFromFilename();
    int getIdCategory();
    void setIdCategory(int id);
    bool Remove();
    
    QImage* getCoverImage();
    QPixmap* getCoverPixmap();
    void setCoverPixmap(QPixmap* pix) { coverPixmap = pix; }
    bool haveCoverPixmap() const { return (coverPixmap != NULL); }
  private:
    void fillCategory();
    void fillCountries();
    void updateCountries();
    void fillGenres();
    void updateGenres();
    QImage* coverImage;
    QPixmap* coverPixmap;
        
    QString title;
    QString inetref;
    QString director;
    QString plot;
    QString rating;
    int childID;
    int year;
    float userrating;
    int length;
    int showlevel;
    bool browse;
    QString playcommand;
    QString category;
    QStringList genres;
    QStringList countries;
    QString player;
    unsigned int id;
    
    QString filename;
    QString coverfile;
};

bool operator==(const Metadata& a, const Metadata& b);
bool operator!=(const Metadata& a, const Metadata& b);

#endif
