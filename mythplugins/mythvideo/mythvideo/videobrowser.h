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

#include "videodlg.h"

class VideoBrowser : public VideoDialog
{
    Q_OBJECT
  
  public:
    VideoBrowser(MythMainWindow *parent, const char *name = 0);
    virtual ~VideoBrowser();
   
  protected slots:
    void cursorLeft();
    void cursorRight();
    
    virtual void slotParentalLevelChanged();

  protected:
    virtual void parseContainer(QDomElement &element);
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void doMenu(bool info=false);
    virtual void fetchVideos();
    
    LayerSet* getContainer(const QString &name);
    
  private:
    QPixmap getPixmap(QString &level);
    void jumpToSelection(int amount);
    void jumpSelection(int amount);
    void SetCurrentItem(unsigned int index);
    void updateInfo(QPainter *);
    void updateBrowsing(QPainter *);
    void updatePlayWait(QPainter *);
    void grayOut(QPainter *);

    QPixmap *bgTransBackup;
    QPainter backup;

    unsigned int inData;	// index of curItem in VideoList.metas
    int m_state;

    QString m_title;
    
    QRect infoRect;
    QRect browsingRect;
};

#endif
