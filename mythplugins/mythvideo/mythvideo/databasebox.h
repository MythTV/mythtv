#ifndef DATABASEBOX_H_
#define DATABASEBOX_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qstringlist.h>

#include "metadata.h"
#include <mythtv/mythwidgets.h>
#include <qdom.h>
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>

class QSqlDatabase;
class QListViewItem;
class TreeCheckItem;
class QLabel;

class DatabaseBox : public MythDialog
{
    Q_OBJECT
  public:
    DatabaseBox(QSqlDatabase *ldb, QValueList<Metadata> *playlist,
                QWidget *parent = 0, const char *name = 0);
   ~DatabaseBox();

  signals:
    void killTheApp();

  protected slots:
    void selected();
    void cursorDown(bool page = false);
    void cursorUp(bool page = false);
    void pageDown() { cursorDown(true); }
    void pageUp() { cursorUp(true); }
    void exitWin();

  protected:
    void paintEvent(QPaintEvent *);

  private:
    QPixmap getPixmap(QString &level);
    QSqlDatabase *db;
    QValueList<Metadata> *m_list;

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    void updateBackground(void);
    void updateList(QPainter *);
    void updateInfo(QPainter *);
    void updatePlayWait(QPainter *);

    void grayOut(QPainter *);
    void resizeImage(QPixmap *, QString);

    QAccel *accel;

    QPixmap *bgTransBackup;
    Metadata *curitem;

    QPainter backup;
    QPixmap myBackground;
    bool pageDowner;

    int inList;
    int inData;
    int listCount;
    int dataCount;

    int m_status;

    int space_itemid;
    int enter_itemid;
    int return_itemid;

    QRect listRect;
    QRect infoRect;
    QRect fullRect;

    int listsize;

    QString m_cmd;   
    QString m_title;
};

#endif
