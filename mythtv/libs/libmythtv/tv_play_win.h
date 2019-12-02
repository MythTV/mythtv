#ifndef TVPLAYWIN_H_
#define TVPLAYWIN_H_

#include "mythscreentype.h"
#include "mythgesture.h"

class MythUIProgressBar;

/** \class TvPlayWindow
 *  \brief Simple screen shown while the video player is starting up
 */
class TvPlayWindow : public MythScreenType
{
    Q_OBJECT

  public:
    TvPlayWindow(MythScreenStack *parent, const char *name);
   ~TvPlayWindow() = default;

    bool gestureEvent(MythGestureEvent *) override; // MythUIType
    bool Create(void) override; // MythScreenType

    void UpdateProgress(void);

  protected:
    MythUIProgressBar *m_progressBar {nullptr};
    int                m_progress    {0};
};

#endif
