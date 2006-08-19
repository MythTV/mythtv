#ifndef VIDEOMANAGER_H_
#define VIDEOMANAGER_H_

#include <memory>

#include <mythtv/mythdialogs.h>

namespace
{
    class ListBehaviorManager;

    enum DisplayState
    {
        SHOWING_MAINWINDOW = 0,
        SHOWING_EDITWINDOW,
        SHOWING_IMDBLIST,
        SHOWING_IMDBMANUAL
    };
}

class QPainter;
class QNetworkOperation;
class VideoList;
class Metadata;
class VideoManager : public MythDialog
{
    Q_OBJECT
  public:
    VideoManager(MythMainWindow *lparent, const QString &lname,
                 VideoList *video_list);
    ~VideoManager();
    int videoExitType() { return 0; }

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
    void videoMenu();

    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();

    void pageDown();
    void pageUp();

    void exitWin();

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);
    void doWaitBackground(QPainter &p, const QString &titleText);

  private slots:
    void num(const QString &text);
    void copyFinished(QNetworkOperation *op);

  private:
    void handleIMDBList();
    void handleIMDBManual();
    void doParental(int amount);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);

    void cancelPopup();

    void ResetCurrentItem();

    void RefreshMovieList(bool resort_only);
    void GetMovieData(const QString &);
    int GetMovieListing(const QString &);
    QString GetMoviePoster(const QString &);

    void updateBackground();
    void updateList(QPainter *);
    void updateMovieList(QPainter *);
    void updateInfo(QPainter *);
    void updateIMDBEnter(QPainter *);

    void grayOut(QPainter *);

  private:
    bool updateML;
    bool noUpdate;

    VideoList *m_video_list;

    std::auto_ptr<XMLParse> m_theme;

    QStringList movieList;
    QString curIMDBNum;

    std::auto_ptr<QPixmap> bgTransBackup;
    Metadata *curitem;
    QString curitemMovie;

    std::auto_ptr<QPainter> backup;
    QPixmap myBackground;

    DisplayState m_state;

    QRect listRect;
    QRect movieListRect;
    QRect infoRect;
    QRect fullRect;
    QRect imdbEnterRect;

    QString movieNumber;

    MythPopupBox *popup;
    bool expectingPopup;

    QString videoDir;
    QString artDir;
    bool allowselect;
    bool isbusy; // ignores keys when true (set when doing http request)
    bool iscopycomplete;
    bool iscopysuccess;

    std::auto_ptr<ListBehaviorManager> m_list_behave;
    std::auto_ptr<ListBehaviorManager> m_movie_list_behave;
};

#endif
