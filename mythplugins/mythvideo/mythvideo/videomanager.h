#ifndef VIDEOMANAGER_H_
#define VIDEOMANAGER_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qapplication.h>
#include <qstringlist.h>

#include "inetcomms.h"
#include "metadata.h"
#include <mythtv/mythwidgets.h>
#include <qdom.h>
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>

class QSqlDatabase;
typedef QValueList<Metadata>  ValueMetadata;

class VideoManager : public MythDialog
{
    Q_OBJECT
  public:
    VideoManager(QSqlDatabase *ldb, 
                 MythMainWindow *parent, const char *name = 0);
    ~VideoManager(void);
    void VideoManager::processEvents() { qApp->processEvents(); }
    

  protected slots:
    void selected();
    void videoMenu();
    void cursorLeft();
    void cursorRight();
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void exitWin();
    void GetMovieListingTimeOut();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private slots:
    void num(QKeyEvent *e);

  private:
    bool updateML;
    bool noUpdate;

    QPixmap getPixmap(QString &level);
    QSqlDatabase *db;
    ValueMetadata m_list;

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    QString parseData(QString, QString, QString);
    QMap<QString, QString> parseMovieList(QString);
    void ResetCurrentItem();

    INETComms *InetGrabber;
    void RefreshMovieList();
    QString ratingCountry;
    void GetMovieData(QString);
    int GetMovieListing(QString);
    QString GetMoviePoster(QString);
    void ParseMovieData(QString);
    QMap<QString, QString> movieList;
    QString curIMDBNum;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateMovieList(QPainter *);
    void updateInfo(QPainter *);
    void updatePlayWait(QPainter *);
    void updateIMDBEnter(QPainter *);

    void grayOut(QPainter *);
    void resizeImage(QPixmap *, QString);

    QPixmap *bgTransBackup;
    Metadata *curitem;
    QString curitemMovie;

    QPainter backup;
    QPixmap myBackground;
    bool pageDowner;
    bool pageDownerMovie;

    int inList;
    int inData;
    int listCount;
    int dataCount;
 
    int inListMovie;
    int inDataMovie;
    int listCountMovie;
    int dataCountMovie;

    int m_state;

    int listsize;
    int listsizeMovie;
    QRect listRect;
    QRect movieListRect;
    QRect infoRect;
    QRect fullRect;
    QRect imdbEnterRect;

    QString m_cmd;   
    QString m_title;

    QString movieTitle;
    int movieYear;
    QString movieDirector;
    QString moviePlot;
    float movieUserRating;
    QString movieRating;
    int movieRuntime;
    QString movieNumber;
    
    QTimer *urlTimer;
    int GetMovieListingTimeoutCounter;
    bool stopProcessing;
    QString theMovieName;

    bool allowselect;
};

#endif
