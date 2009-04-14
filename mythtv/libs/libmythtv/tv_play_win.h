#ifndef TVPLAYWIN_H_
#define TVPLAYWIN_H_

#include "mythscreentype.h"

/** \class TvPlayWindow
 *  \brief Simple screen shown while the video player is starting up
 */
class TvPlayWindow : public MythScreenType
{
    Q_OBJECT

  public:
    TvPlayWindow(MythScreenStack *parent, const char *name);
   ~TvPlayWindow();

    virtual bool Create(void);
};

#endif
