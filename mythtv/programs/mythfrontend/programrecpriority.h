#ifndef PROGRAMRECPROIRITY_H_
#define PROGRAMRECPROIRITY_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"
#include "scheduledrecording.h"


class ProgramRecPriorityInfo : public ProgramInfo
{
  public:
    ProgramRecPriorityInfo();
    ProgramRecPriorityInfo(const ProgramRecPriorityInfo &other);
    ProgramRecPriorityInfo& operator=(const ProgramInfo&);


    int recTypeRecPriority;
    RecordingType recType;
    int matchCount;
    int recCount;
    QDateTime last_record;
};

class ProgramRecPriority : public MythDialog
{
    Q_OBJECT
  public:
    enum SortType
    {
        byTitle,
        byRecPriority,
        byRecType,
        byCount,
        byRecCount,
        byLastRecord
    };

    ProgramRecPriority(MythMainWindow *parent, const char *name = 0);
    ~ProgramRecPriority();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void changeRecPriority(int howMuch);
    void saveRecPriority(void);
    void edit();
    void customEdit();
    void deactivate();
    void upcoming();

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void FillList(void);
    void SortList(void);
    QMap<QString, ProgramRecPriorityInfo> programData;
    QMap<int, int> origRecPriorityData;

    void countMatches(void);
    QMap<int, int> conMatch;
    QMap<int, int> nowMatch;
    QMap<int, int> recMatch;
    QMap<int, int> listMatch;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    ProgramRecPriorityInfo *curitem;

    QPixmap myBackground;
    QPixmap *bgTransBackup;

    bool pageDowner;
    bool reverseSort;

    int inList;
    int inData;
    int listCount;
    int dataCount;

    QRect listRect;
    QRect infoRect;
    QRect fullRect;

    int listsize;

    SortType sortType;
};

#endif
