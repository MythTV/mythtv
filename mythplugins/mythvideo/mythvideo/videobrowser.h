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

typedef QValueList<Metadata>  ValueMetadata;

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
    virtual void handleMetaFetch(Metadata* meta);
    
    LayerSet* getContainer(const QString &name);
    
  private:
    QPixmap getPixmap(QString &level);
    void jumpSelection(int amount);
    void RefreshMovieList();
    void SetCurrentItem();
    void updateInfo(QPainter *);
    void updateBrowsing(QPainter *);
    void updatePlayWait(QPainter *);
    void grayOut(QPainter *);

    bool updateML;
    bool allowselect;
           
    ValueMetadata m_list;
    
    QPixmap *bgTransBackup;
    

    QPainter backup;

    int inData;
    int m_state;

    QString m_title;
    QString curitemMovie;
    
    QRect infoRect;
    QRect browsingRect;
};

#endif
