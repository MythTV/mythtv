#ifndef RANKPROGRAMS_H_
#define RANKPROGRAMS_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"
#include "scheduledrecording.h"

class QSqlDatabase;

class ProgramRankInfo : public ProgramInfo
{
  public:
    ProgramRankInfo();
    ProgramRankInfo(const ProgramRankInfo &other);
    ProgramRankInfo& operator=(const ProgramInfo&);


    int channelRank;
    int recTypeRank;
    RecordingType recType;
};

class RankPrograms : public MythDialog
{
    Q_OBJECT
  public:
    enum SortType
    {
        byTitle,
        byRank,
    };

    RankPrograms(QSqlDatabase *ldb, MythMainWindow *parent, 
                 const char *name = 0);
    ~RankPrograms();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void changeRank(int howMuch);
    void saveRank(void);
    void edit();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void FillList(void);
    void SortList();
    QMap<QString, ProgramRankInfo> programData;
    QMap<QString, QString> origRankData;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);

    void resizeImage(QPixmap *, QString);
    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    ProgramRankInfo *curitem;

    QSqlDatabase *db;

    QPixmap myBackground;
    QPixmap *bgTransBackup;

    bool pageDowner;

    int inList;
    int inData;
    int listCount;
    int dataCount;

    QRect listRect;
    QRect infoRect;
    QRect fullRect;

    int listsize;

    SortType sortType;

    bool allowKeys;
    bool doingSel;
};

#endif
