#ifndef METADATA_H_
#define METADATA_H_

#include <qregexp.h>
#include <qstring.h>

class QSqlDatabase;

class Metadata
{
  public:
    Metadata(QString lfilename = "", QString lcoverfile = "", 
             QString ltitle = "", int lyear = 0, QString linetref = "", 
             QString ldirector = "", QString lplot = "", 
             float luserrating = 0.0, QString lrating = "", int llength = 0, 
             int lid = 0, int lshowlevel = 1, unsigned int lchildID = 0,
             bool lbrowse = true, QString lplaycommand = "")
    {
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
    }

    Metadata(const Metadata &other) 
    {
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
    }

   ~Metadata() {}

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

    unsigned int ChildID() { return childID; }
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

    void guessTitle();
    void setField(QString field, QString data);
    void dumpToDatabase(QSqlDatabase *db);
    void updateDatabase(QSqlDatabase *db);
    void fillData(QSqlDatabase *db);
    void fillDataFromID(QSqlDatabase *db);

  private:
    QString title;
    QString inetref;
    QString director;
    QString plot;
    QString rating;
    unsigned int childID;
    int year;
    float userrating;
    int length;
    int showlevel;
    bool browse;
    QString playcommand;

    unsigned int id;
    
    QString filename;
    QString coverfile;
};

bool operator==(const Metadata& a, const Metadata& b);
bool operator!=(const Metadata& a, const Metadata& b);

#endif
