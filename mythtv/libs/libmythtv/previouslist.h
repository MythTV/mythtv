#ifndef PREVIOUSLIST_H_
#define PREVIOUSLIST_H_

#include <qdatetime.h>
#include "libmyth/uitypes.h"
#include "libmyth/xmlparse.h"
#include "libmyth/mythwidgets.h"
#include "libmyth/mythdialogs.h"
#include "libmythtv/programinfo.h"


class PreviousList : public MythDialog
{
    Q_OBJECT

  public:
    PreviousList(MythMainWindow *parent, const char *name = 0,
                int recid = 0, QString ltitle = "");
    ~PreviousList();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void prevView(void);
    void nextView(void);
    void setViewFromList(void);
    void select(void);
    void edit(void);
    void customEdit(void);
    void upcoming(void);
    void details(void);
    void chooseView(void);
    void removalDialog(void);
    void deleteItem(void);

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void customEvent(QCustomEvent *e);

  private:
    int m_recid;
    QString m_title;

    QString view;
    QDateTime startTime;
    QDateTime searchTime;
    QString dayFormat;
    QString hourFormat;
    QString timeFormat;
    QString fullDateFormat;
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
