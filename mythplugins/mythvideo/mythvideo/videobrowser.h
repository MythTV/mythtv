#ifndef VIDEOBROWSER_H_
#define VIDEOBROWSER_H_

#include <qwidget.h>
#include <qdialog.h>
#include <qapplication.h>
#include <qstringlist.h>

#include "metadata.h"
#include <mythtv/mythwidgets.h>
#include <qdom.h>
#include <mythtv/uitypes.h>
#include <mythtv/xmlparse.h>

class QSqlDatabase;
typedef QValueList<Metadata>  ValueMetadata;

class VideoBrowser : public MythDialog
{
    Q_OBJECT
  public:
    VideoBrowser(QSqlDatabase *ldb, 
                 QWidget *parent = 0, const char *name = 0);
    ~VideoBrowser();
    void VideoBrowser::processEvents() { qApp->processEvents(); }
    

  signals:
    void killTheApp();

  protected slots:
    void selected();
    void cursorLeft();
    void cursorRight();
    void cursorDown();
    void cursorUp();
    void exitWin();

  protected:
    void paintEvent(QPaintEvent *);

  private:
    bool updateML;
    bool noUpdate;

    QPixmap getPixmap(QString &level);
    QSqlDatabase *db;
    ValueMetadata m_list;

    void LoadWindow(QDomElement &);
    void parseContainer(QDomElement &);
    XMLParse *theme;
    QDomElement xmldata;

    int currentParentalLevel;
    void RefreshMovieList();
    void SetCurrentItem();

    void updateBackground(void);
    void updateInfo(QPainter *);
    void updateBrowsing(QPainter *);
    void updatePlayWait(QPainter *);

    void grayOut(QPainter *);
    void resizeImage(QPixmap *, QString);

    QAccel *accel;

    QPixmap *bgTransBackup;
    Metadata *curitem;
    QString curitemMovie;

    QPainter backup;
    QPixmap myBackground;

    int inData;
 
    int m_state;
    QString m_title;
    QString m_cmd;

    int space_itemid;
    int enter_itemid;
    int return_itemid;

    QRect infoRect;
    QRect browsingRect;
    QRect fullRect() const;

};

#endif
