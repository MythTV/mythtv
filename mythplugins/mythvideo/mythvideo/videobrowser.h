#ifndef VIDEOBROWSER_H_
#define VIDEOBROWSER_H_

#include <QPixmap>

#include "videodlg.h"

class QPainter;
class QPaintEvent;
class QKeyEvent;

class VideoBrowser : public VideoDialog
{
    Q_OBJECT

  public:
    VideoBrowser(MythMainWindow *lparent, const QString &lname,
                 VideoList *video_list);
    virtual ~VideoBrowser();

  protected slots:
    void cursorLeft();
    void cursorRight();

    void slotParentalLevelChanged();

  protected:
    void parseContainer(QDomElement &element);
    void paintEvent(QPaintEvent *);
    void keyPressEvent(QKeyEvent *e);
    void doMenu(bool info = false);
    void fetchVideos();

  private:
    QPixmap getPixmap(QString &level);
    void jumpToSelection(int amount);
    void jumpSelection(int amount);
    void SetCurrentItem(unsigned int index);
    void updateInfo(QPainter *);
    void updateBrowsing(QPainter *);
    void updatePlayWait(QPainter *);

  private:
    std::auto_ptr<QPixmap> bgTransBackup;

    unsigned int inData; // index of curItem in VideoList.metas
    int m_state;

    QRect infoRect;
    QRect browsingRect;
};

#endif
