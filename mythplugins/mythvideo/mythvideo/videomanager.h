#ifndef VIDEOMANAGER_H_
#define VIDEOMANAGER_H_

#include <mythtv/mythdialogs.h>

#include <memory>

namespace mythvideo_videomanager { class VideoManagerImp; };

class VideoList;
class VideoManager : public MythDialog
{
    Q_OBJECT

  public:
    VideoManager(MythMainWindow *lparent, const QString &lname,
                 VideoList *video_list);

    int videoExitType() { return 0; }

  protected slots:
    void ExitWin();

  protected:
    ~VideoManager(); // use deleteLater() instead for thread safety

    void paintEvent(QPaintEvent *event_);
    void keyPressEvent(QKeyEvent *event_);

  private:
    std::auto_ptr<mythvideo_videomanager::VideoManagerImp> m_imp;
};

#endif
