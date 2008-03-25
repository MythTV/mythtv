#ifndef MOVIESUI_H_
#define MOVIESUI_H_

#include <mythtv/mythdialogs.h>
#include <mythtv/mythdbcon.h>

#include "helperobjects.h"
//Added by qt3to4:
#include <QKeyEvent>


class QTimer;

class MoviesUI : public MythThemedDialog
{
    Q_OBJECT
  public:
    typedef Q3ValueVector<int> IntVector;

    MoviesUI(MythMainWindow *parent, QString windowName,
             QString themeFilename, const char *name = 0);
    ~MoviesUI();

  protected:
    void keyPressEvent(QKeyEvent *e);
    void showAbout();
    void showMenu();
  private:
    void updateDataTrees();
    void updateMovieTimes();
    void setupTheme(void);
    TheaterVector loadTrueTreeFromFile(QString);
    void drawDisplayTree();
    GenericTree* getDisplayTreeByMovie();
    GenericTree* getDisplayTreeByTheater();
    void populateDatabaseFromGrabber(QString ret);
    void processTheatre(QDomNode &n);
    void processMovie(QDomNode &n, int theaterId);
    TheaterVector buildTheaterDataTree();
    MovieVector buildMovieDataTree();
    TheaterVector m_dataTreeByTheater;
    Theater *m_currentTheater;
    MovieVector m_dataTreeByMovie;
    Movie *m_currentMovie;
    GenericTree           *m_movieTree;
    UIManagedTreeListType *m_movieTreeUI;
    GenericTree *m_currentNode;
    QString m_currentMode;
    QTimer *waitForReady;
    MSqlQuery *query;
    MSqlQuery *subQuery;

    UITextType  *m_movieTitle;
    UITextType  *m_movieRating;
    UITextType  *m_movieRunningTime;
    UITextType  *m_movieShowTimes;
    UITextType  *m_theaterName;
    MythPopupBox *aboutPopup;
    MythPopupBox *menuPopup;
    QAbstractButton *OKButton;
    QAbstractButton *updateButton;

  public slots:
    void handleTreeListSelection(int, IntVector*);
    void handleTreeListEntry(int, IntVector*);

  protected slots:
    void closeAboutPopup();
    void closeMenu();
    void slotUpdateMovieTimes();
};

#endif
