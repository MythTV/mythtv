#ifndef RANKCHANNELS_H_
#define RANKCHANNELS_H_

#include <qdatetime.h>
#include <qdom.h>
#include "mythwidgets.h"
#include "mythdialogs.h"
#include "uitypes.h"
#include "xmlparse.h"
#include "programinfo.h"

class QSqlDatabase;
class ChannelInfo;

class RankChannels : public MythDialog
{
    Q_OBJECT
  public:
    enum SortType
    {
        byChannel,
        byRank,
    };

    RankChannels(QSqlDatabase *ldb, MythMainWindow *parent, 
                  const char *name = 0);
    ~RankChannels();

  protected slots:
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void changeRank(int howMuch);
    void applyChannelRankChange(QSqlDatabase*, QString, const QString&);

    void saveRank(void);

  protected:
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);

  private:
    void FillList(void);
    void SortList();
    QMap<QString, ChannelInfo> channelData;
    QMap<QString, QString> origRankData;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    ChannelInfo *curitem;

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
};

#endif
