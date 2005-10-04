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
    Metadata(const QString& lfilename = "", const QString& lcoverfile = "", 
             const QString& ltitle = "", int lyear = 0, const QString& linetref = "", 
             const QString& ldirector = "", const QString& lplot = "", 
             float luserrating = 0.0, const QString& lrating = "", int llength = 0, 
             int lid = 0, int lshowlevel = 1, int lchildID = -1,
             bool lbrowse = true, const QString& lplaycommand = "",
             const QString& lcategory = "",
             const QStringList& lgenres = QStringList(),
             const QStringList& lcountries = QStringList())
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
    
    Metadata(const Metadata &other) { clone(other); }
    
    void clone(const Metadata &other) 
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
        
        coverImage  = NULL;
        coverPixmap = NULL;
        
        filename    = "";
        coverfile   = "";
        title       = "";
        inetref     = "";
        director    = "";
        plot        = "";
        playcommand = "";
        category    = "";
        rating      = "";
        
        length      = 0;
        showlevel   = 1;
        id          = 0;
        categoryID  = 0;
        childID     = -1;
        year        = 0;
        
        userrating  = 0.0;
        
        browse = true;
       
        genres = QStringList();
        countries = QStringList();
        player = "";

    }
    
    ~Metadata() { if (coverImage) delete coverImage; }


    const QString& Title() const { return title; }
    void setTitle(const QString& _title) { title = _title; }
    
    int Year() const { return year; }
    void setYear(int _year) { year = _year; }

    const QString& InetRef() const { return inetref; }
    void setInetRef(const QString& _inetRef) { inetref = _inetRef; }

    const QString& Director() const { return director; }
    void setDirector(const QString& _director) { director = _director; }

    const QString& Plot() const { return plot; }
    void setPlot(const QString& _plot) { plot = _plot; }

    float UserRating() const { return userrating; }
    void setUserRating(float _userRating) { userrating = _userRating; }
 
    const QString& Rating() const { return rating; }
    void setRating(const QString& _rating) { rating = _rating; }

    int Length() const { return length; }
    void setLength(int _length) { length = _length; }

    unsigned int ID() const { return id; }
    void setID(int _id) { id = _id; }

    int ChildID() const { return childID; }
    void setChildID(int _childID) { childID = _childID; }
    
    bool Browse() const {return browse; }
    void setBrowse(bool _browse){ browse = _browse;}
   
    const QString& PlayCommand() const {return playcommand;}
    void setPlayCommand(const QString& _playCommand){playcommand = _playCommand;}
    
    int ShowLevel() const { return showlevel; }
    void setShowLevel(int _showLevel) { showlevel = _showLevel; }

    const QString& Filename() const { return filename; }
    void setFilename(QString& _filename) { filename = _filename; }

    const QString& CoverFile() const { return coverfile; }
    void setCoverFile(QString& _coverFile) { coverfile = _coverFile; }
    
    const QString& Player() const { return player; }
    void setPlayer(const QString& _player) { player = _player; }

    const QString& Category() const { return category;}
    void setCategory(const QString& _category) { category = _category;}
    
    const QStringList& Genres() const { return genres; }
    void setGenres(const QStringList& _genres) { genres = _genres; }

    const QStringList& Countries() const { return countries;}
    void setCountries(const QStringList& _countries) { countries = _countries; }

    int getCategoryID() { if (categoryID <= 0) categoryID = lookupCategoryID(); return categoryID; }
    void setCategoryID(int id);
    
    QPixmap* getCoverPixmap();
    void setCoverPixmap(QPixmap* pix) { coverPixmap = pix; }
    bool haveCoverPixmap() const { return (coverPixmap != NULL); }

    QImage* getCoverImage();
            
    void guessTitle();
    void eatBraces(const QString& left_brace, const QString& right_brace);

    void dumpToDatabase();
    void updateDatabase();
    bool fillDataFromID();
    bool fillDataFromFilename();
    
    bool Remove();
  
  private:
    void fillCategory();
    void fillCountries();
    void updateCountries();
    void fillGenres();
    void updateGenres();
    bool removeDir(const QString& dirName);
    int lookupCategoryID();  
    
    
    
    QImage* coverImage;
    QPixmap* coverPixmap;
    

    QString title;
    QString inetref;
    QString director;
    QString plot;
    QString rating;
    QString playcommand;
    QString category;
    QStringList genres;
    QStringList countries;
    QString player;
    QString filename;
    QString coverfile;

    int categoryID;
    int childID;
    int year;
    int length;
    int showlevel;
    bool browse;
    unsigned int id;	// videometadata.intid
    float userrating;    
};

bool operator==(const Metadata& a, const Metadata& b);
bool operator!=(const Metadata& a, const Metadata& b);

#endif
