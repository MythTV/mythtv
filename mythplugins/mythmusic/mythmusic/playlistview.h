#ifndef PLAYLISTVIEW_H_
#define PLAYLISTVIEW_H_

// qt
#include <QEvent>
#include <QVector>

// mythui
#include <mythscreentype.h>

// mythmusic
#include <musiccommon.h>

class MythUIButtonList;
class MythUIText;

class PlaylistView : public MusicCommon
{
    Q_OBJECT
  public:
    PlaylistView(MythScreenStack *parent, MythScreenType *parentScreen);
    ~PlaylistView(void);

    bool Create(void);
    bool keyPressEvent(QKeyEvent *);

  protected:
    void customEvent(QEvent *event);
};

#endif
