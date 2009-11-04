#ifndef MOVIESUI_H_
#define MOVIESUI_H_

// qt
#include <QVector>
#include <QKeyEvent>

// myth
#include <mythscreentype.h>
#include <mythdbcon.h>

// mythmovies
#include "helperobjects.h"

class QTimer;
class QDomNode;
class MythUIText;
class MythUIButtonTree;
class MythGenericTree;

class MoviesUI : public MythScreenType
{
    Q_OBJECT

  public:
    typedef QVector<int> IntVector;

    MoviesUI(MythScreenStack *parentStack);
    ~MoviesUI();

    bool Create();
    bool keyPressEvent(QKeyEvent *event);

  protected:
    void showMenu();

  private:
    virtual void Load();
    virtual void Init();

    void updateDataTrees();
    void updateMovieTimes();
    void setupTheme(void);
    TheaterVector loadTrueTreeFromFile(QString);
    void drawDisplayTree();
    MythGenericTree* getDisplayTreeByMovie();
    MythGenericTree* getDisplayTreeByTheater();
    bool populateDatabaseFromGrabber(QString ret);
    void processTheatre(QDomNode &n);
    void processMovie(QDomNode &n, int theaterId);
    TheaterVector buildTheaterDataTree();
    MovieVector buildMovieDataTree();

    TheaterVector m_dataTreeByTheater;
    Theater       m_currentTheater;
    MovieVector   m_dataTreeByMovie;
    Movie         m_currentMovie;
    MythGenericTree  *m_movieTree;
    MythGenericTree  *m_currentNode;
    QString       m_currentMode;

    MythUIButtonTree *m_movieTreeUI;
    MythUIText  *m_movieTitle;
    MythUIText  *m_movieRating;
    MythUIText  *m_movieRunningTime;
    MythUIText  *m_movieShowTimes;
    MythUIText  *m_theaterName;

  public slots:
    void nodeChanged(MythGenericTree* node);

  protected slots:
    void slotUpdateMovieTimes();
};

#endif
