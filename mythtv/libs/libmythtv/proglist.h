#ifndef PROGLIST_H_
#define PROGLIST_H_

#include <qdatetime.h>
#include "libmyth/uitypes.h"
#include "libmyth/xmlparse.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythdialogs.h"
#include "libmythtv/programinfo.h"

enum ProgListType {
    plUnknown = 0,
    plTitle = 1,
    plNewListings = 2,
    plTitleSearch = 3,
    plKeywordSearch = 4,
    plChannel = 5,
    plCategory = 6,
    plMovies = 7,
    plPeopleSearch = 8,
    plTime = 9
};

class QSqlDatabase;

class ProgLister : public MythDialog
{
    Q_OBJECT

  public:
    ProgLister(ProgListType pltype, const QString &view, QSqlDatabase *ldb, 
	       MythMainWindow *parent, const char *name = 0);
    ~ProgLister();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void prevView(void);
    void nextView(void);
    void setViewFromList(void);
    void chooseEditChanged(void);
    void chooseListBoxChanged(void);
    void setViewFromEdit(void);
    void addSearchRecord(void);
    void deleteKeyword(void);
    void setViewFromTime(void);
    void select(void);
    void edit(void);
    void upcoming(void);
    void details(void);
    void chooseView(void);


  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);
    void quickRecord(void);

  private:
    ProgListType type;
    QSqlDatabase *db;
    QDateTime startTime;
    QDateTime searchTime;
    QString dayFormat;
    QString hourFormat;
    QString timeFormat;
    QString fullDateFormat;
    QString channelOrdering;
    QString channelFormat;

    RecSearchType searchtype;

    int curView;
    QStringList viewList;
    QStringList viewTextList;

    int curItem;
    ProgramList itemList;
    ProgramList schedList;

    XMLParse *theme;
    QDomElement xmldata;

    QRect viewRect;
    QRect listRect;
    QRect infoRect;
    QRect fullRect;
    int listsize;

    bool allowEvents;
    bool allowUpdates;
    bool updateAll;
    bool refillAll;

    void updateBackground(void);
    void updateView(QPainter *);
    void updateList(QPainter *);
    void updateInfo(QPainter *);
    void fillViewList(const QString &view);
    void fillItemList(void);
    void LoadWindow(QDomElement &);

    void createPopup(void);
    void deletePopup(void);

    MythPopupBox *choosePopup;
    MythListBox *chooseListBox;
    MythRemoteLineEdit *chooseLineEdit;
    MythPushButton *chooseOkButton;
    MythPushButton *chooseDeleteButton;
    MythPushButton *chooseRecordButton;
    MythComboBox *chooseDay;
    MythComboBox *chooseHour;
};

#endif
