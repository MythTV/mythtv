// -*- Mode: c++ -*-

#ifndef PROGLIST_QT_H_
#define PROGLIST_QT_H_

// Qt headers
#include <QDateTime>
#include <QEvent>
#include <QPaintEvent>
#include <QKeyEvent>

// MythTV headers
#include "uitypes.h"
#include "xmlparse.h"
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "programinfo.h"
#include "programlist.h"

enum ProgListTypeQt {
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
    plTime,
    plRecordid,
    plStoredSearch
};

class MPUBLIC ProgListerQt : public MythDialog
{
    Q_OBJECT

  public:
    ProgListerQt(ProgListTypeQt pltype, const QString &view, const QString &from,
               MythMainWindow *parent, const char *name = 0);
    ~ProgListerQt();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void prevView(void);
    void nextView(void);
    void setViewFromList(int);
    void chooseEditChanged(void);
    void chooseListBoxChanged(void);
    void setViewFromEdit(void);
    void addSearchRecord(void);
    void deleteKeyword(void);
    void setViewFromTime(void);
    void select(void);
    void edit(void);
    void customEdit(void);
    void remove(void);
    void upcoming(void);
    void details(void);
    void chooseView(void);
    void powerEdit(void);
    void setViewFromPowerEdit(void);


  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QEvent *e);
    void quickRecord(void);

  private:
    ProgListTypeQt type;
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
    QStringList genreList;
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
    bool useGenres;

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
    MythComboBox *powerGenre;
    MythComboBox *powerStation;
    MythPushButton *powerOkButton;

    bool powerStringToSQL(const QString &qphrase, QString &output, 
                          MSqlBindings &bindings);
};

#endif
