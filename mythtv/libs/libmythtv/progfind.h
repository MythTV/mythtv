/*
        MythProgramFind
        Version 0.8
        January 19th, 2003
        Updated: 4/8/2003, Added support for new ui.xml 

        By John Danner

        Note: Portions of this code taken from MythMusic
*/


#ifndef PROGFIND_H_
#define PROGFIND_H_

#include <qdatetime.h>

#include "libmyth/uitypes.h"
#include "libmyth/xmlparse.h"
#include "libmyth/mythwidgets.h"
#include "programinfo.h"

class QListView;
class ProgramInfo;
class QWidget;
class TV;

void RunProgramFind(bool thread = false, bool ggActive = false);

class ProgFinder : public MythDialog
{
    Q_OBJECT
  public:
    ProgFinder(MythMainWindow *parent, const char *name = 0, bool gg = false);
    virtual ~ProgFinder();

    void Initialize(void);

  private slots:
    void update_timeout();
    void escape();
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();
    void getInfo(bool toggle = false);
    void showGuide();
    void pageUp();
    void pageDown();
    void select();
    void customEdit();
    void upcoming();
    void details();
    void quickRecord();
    void customEvent(QCustomEvent *e);

  protected:
    void paintEvent(QPaintEvent *e);
    void keyPressEvent(QKeyEvent *e);

    virtual void fillSearchData(void);
    virtual void getAllProgramData(void);
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(int canNum, QString &where, 
                                          MSqlBindings &bindings);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void updateBackground();
    void updateList(QPainter *);
    void updateInfo(QPainter *);
   
    int showsPerListing;
    int curSearch;
    int curProgram;
    int curShow;
    int searchCount;
    int listCount;
    int showCount;
    int inSearch;
    bool showInfo;
    bool pastInitial;
    bool running;
    int *gotInitData;
    bool ggActive;
    bool arrowAccel;

    QTimer *update_Timer;

    ProgramList showData;
    ProgramList schedList;

    TV *m_player;

    QString baseDir;
    QString *searchData;
    QString *initData;
    QString *progData;

    void showSearchList();
    void showProgramList();
    void showShowingList();
    void clearProgramList();
    void clearShowData();
    void selectSearchData();
    void selectShowData(QString, int);
    void getSearchData(int);
    void getInitialProgramData();

    QRect listRect;
    QRect infoRect;

    QString dateFormat;
    QString timeFormat;
    QString channelFormat;

    bool allowkeypress;
    bool inFill;
    bool needFill;
};

class JaProgFinder : public ProgFinder
{
  public:
    JaProgFinder(MythMainWindow *parent, const char *name = 0, bool gg=false);

  protected:
    virtual void fillSearchData();
    virtual void getAllProgramData();
    virtual bool formatSelectedData(QString &data);
    virtual bool formatSelectedData(QString &data, int charNum);
    virtual void restoreSelectedData(QString &data);
    virtual void whereClauseGetSearchData(int canNum, QString &where,
                                          MSqlBindings &bindings);

  private:
    static const char* searchChars[];
    int numberOfSearchChars;
};

#endif
