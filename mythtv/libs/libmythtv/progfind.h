/*
        MythProgramFind
        Version 0.8
        January 19th, 2003

        By John Danner

        Note: Portions of this code taken from MythMusic


*/


#ifndef PROGFIND_H_
#define PROGFIND_H_

#include <qsqldatabase.h>
#include <qsocket.h>
#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>
#include <qlayout.h>

#include "libmyth/mythwidgets.h"
#include "guidegrid.h"
#include "tv.h"

class QLabel;
class QListView;
class ProgramInfo;

void RunProgramFind(bool thread = false);

class ProgFinder : public MythDialog
{
  struct showRecord {

        QString title;
        QString subtitle;
        QString description;
        QString channelID;
        QString startDisplay;
        QString endDisplay;
        QString channelNum;
        QString channelCallsign;
        QString starttime;
        QString endtime;
	QDateTime startdatetime;
	int recording;
	QString recText;

};

struct recordingRecord {

        QString chanid;
        QDateTime startdatetime;
        QString title;
        QString subtitle;
        QString description;
        int type;

};
    Q_OBJECT
  public:
    ProgFinder(QWidget *parent = 0, const char *name = 0);
    ~ProgFinder();

  protected:
    void hideEvent(QHideEvent *e);

  signals:
    void killTheApp();

  private slots:
    void update_timeout();
    void escape();
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();
    void getInfo();
    void showGuide();
    void pageUp();
    void pageDown();

  private:
    int showProgramBar;
    int showsPerListing;
    int curSearch;
    int curProgram;
    int curShow;
    int recordingCount;
    int searchCount;
    int listCount;
    int showCount;
    int inSearch;
    bool showInfo;
    bool pastInitial;
    bool running;
    int *gotInitData;

    QTimer *update_Timer;

    QFrame *topFrame;
    QFrame *programFrame;

    QVBoxLayout *main;
    QHBoxLayout *topDataLayout;
    QHBoxLayout *topFrameLayout;

    showRecord *showData;
    recordingRecord *curRecordings;

    QAccel *accel;
    QLabel *dTime;

    QLabel *abcList;
    QLabel *progList;
    QLabel *showList;
 
    TV *m_player;

    QLabel *programTitle;
    QLabel *subTitle;
    QLabel *description;
    QLabel *callChan;
    QLabel *recordingInfo;

    QString baseDir;
    QString curDateTime;
    QString curChannel;
    QString *searchData;
    QString *initData;
    QString *progData;

    QSqlDatabase *m_db;

    QColor curProg_bgColor;
    QColor curProg_dsColor;
    QColor curProg_fgColor;
    QColor time_bgColor;
    QColor time_dsColor;
    QColor time_fgColor;
    QColor prog_bgColor;
    QColor prog_fgColor;
    QColor abc_bgColor;
    QColor abc_fgColor;
    QColor recording_bgColor;
    QColor progFindMid_bgColor;
    QColor progFindMid_fgColor;
    QColor misChanIcon_bgColor;
    QColor misChanIcon_fgColor;

    void setupColorScheme();
    void setupLayout();
    void showSearchList();
    void showProgramList();
    void showShowingList();
    void clearProgramList();
    void clearShowData();
    void selectSearchData();
    void selectShowData(QString);
    int checkRecordingStatus(int);
    void getRecordingInfo();
    void getSearchData(int);
    void getInitialProgramData();
};

#endif
