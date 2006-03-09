#ifndef VIDEOMANAGER_H_
#define VIDEOMANAGER_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qapplication.h>
#include <qstringlist.h>

#include <mythtv/httpcomms.h>
#include "metadata.h"
#include <mythtv/mythwidgets.h>
#include <qdom.h>
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>
#include "videofilter.h"

enum
{
    SHOWING_MAINWINDOW = 0,
    SHOWING_EDITWINDOW,
    SHOWING_IMDBLIST,
    SHOWING_IMDBMANUAL
};

typedef QValueList<Metadata>  ValueMetadata;

class VideoManager : public MythDialog
{
    Q_OBJECT
  public:
    VideoManager(MythMainWindow *parent, const char *name = 0);
    ~VideoManager(void);
    void processEvents() { qApp->processEvents(); }
    
  public slots:
    void slotManualIMDB();
    void slotAutoIMDB();
    void slotEditMeta();
    void slotRemoveVideo();
    void slotResetMeta();
    void slotDoCancel();
    void slotDoFilter();
    void slotToggleBrowseable();
    
    
  
  protected slots:
    void selected();
    void videoMenu();
    void editMetadata();
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();
    void pageDown();
    void pageUp();
    void exitWin();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void validateUp();
    void doWaitBackground(QPainter& p, const QString& titleText);

  private slots:
    void num(const QString &text);
    void copyFinished(QNetworkOperation *op);

  private:
    void handleIMDBList();
    void handleIMDBManual();
    void doParental(int amount);
    
    bool updateML;
    bool noUpdate;
    int debug;
    VideoFilterSettings *currentVideoFilter;

    QPixmap getPixmap(QString &level);
    ValueMetadata m_list;

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);

    void cancelPopup();
    
    XMLParse *theme;
    QDomElement xmldata;

    void ResetCurrentItem();
    void RemoveCurrentItem();

    void RefreshMovieList();
    QString ratingCountry;
    void GetMovieData(const QString& );
    int GetMovieListing(const QString& );
    QString GetMoviePoster(const QString& );
    QStringList movieList;
    QString curIMDBNum;
    QString executeExternal(const QStringList& args, const QString& purpose = QString(""));

    void updateBackground(void);
    void updateList(QPainter *);
    void updateMovieList(QPainter *);
    void updateInfo(QPainter *);
    void updatePlayWait(QPainter *);
    void updateIMDBEnter(QPainter *);

    void grayOut(QPainter *);

    QPixmap *bgTransBackup;
    Metadata *curitem;
    QString curitemMovie;

    QPainter backup;
    QPixmap myBackground;
    bool can_do_page_down;
    bool can_do_page_down_movie;

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
    QStringList movieGenres;
    QStringList movieCountries;

    MythPopupBox* popup;
    bool expectingPopup;
    
    QString videoDir;    
    QString artDir;
    QString theMovieName;
    bool allowselect;
    bool isbusy;
    bool iscopycomplete;
    bool iscopysuccess;
};

#endif
