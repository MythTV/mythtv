#ifndef CHANNELRECPRIORITY_H_
#define CHANNELRECPRIORITY_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"

class ChannelInfo;

class ChannelRecPriority : public MythDialog
{
    Q_OBJECT
  public:
    enum SortType
    {
        byChannel,
        byRecPriority,
    };

    ChannelRecPriority(MythMainWindow *parent, const char *name = 0);
    ~ChannelRecPriority();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void changeRecPriority(int howMuch);
    void applyChannelRecPriorityChange(QString, const QString&);

    void saveRecPriority(void);

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void FillList(void);
    void SortList();
    QMap<QString, ChannelInfo> channelData;
    QMap<QString, QString> origRecPriorityData;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    ChannelInfo *curitem;

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

    QString longchannelformat;
};

#endif
