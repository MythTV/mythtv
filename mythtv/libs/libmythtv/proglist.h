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
    plTitleSearch,
    plKeywordSearch,
    plPeopleSearch,
    plPowerSearch,
    plSQLSearch,
    plNewListings,
    plMovies,
    plCategory,
    plChannel,
    plTime
};


class ProgLister : public MythDialog
{
    Q_OBJECT

  public:
    ProgLister(ProgListType pltype, const QString &view, const QString &from,
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
    void customEdit(void);
    void upcoming(void);
    void details(void);
    void chooseView(void);
    void powerEdit(void);
    void setViewFromPowerEdit(void);


  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);
    void quickRecord(void);

  private:
    ProgListType type;
    QString addTables;
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

    QStringList typeList;
    QStringList categoryList;
    QStringList stationList;

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
    bool titleSort;
    bool reverseSort;

    void updateBackground(void);
    void updateView(QPainter *);
    void updateList(QPainter *);
    void updateInfo(QPainter *);
    void fillViewList(const QString &view);
    void fillItemList(void);
    void LoadWindow(QDomElement &);

    void updateKeywordInDB(const QString &text);

    void createPopup(void);
    void deletePopup(void);

    MythPopupBox *choosePopup;
    MythListBox *chooseListBox;
    MythRemoteLineEdit *chooseLineEdit;
    MythPushButton *chooseEditButton;
    MythPushButton *chooseOkButton;
    MythPushButton *chooseDeleteButton;
    MythPushButton *chooseRecordButton;
    MythComboBox *chooseDay;
    MythComboBox *chooseHour;

    MythPopupBox *powerPopup;
    MythRemoteLineEdit *powerTitleEdit;
    MythRemoteLineEdit *powerSubtitleEdit;
    MythRemoteLineEdit *powerDescEdit;
    MythComboBox *powerCatType;
    MythComboBox *powerCategory;
    MythComboBox *powerStation;
    MythPushButton *powerOkButton;

    void powerStringToSQL(const QString &qphrase, QString &output, 
                          MSqlBindings &bindings);
};

#endif
