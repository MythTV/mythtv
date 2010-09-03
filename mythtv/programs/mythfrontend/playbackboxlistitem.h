// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef _PLAYBACKBOXLISTITEM_H_
#define _PLAYBACKBOXLISTITEM_H_


#include "mythuibuttonlist.h"

class PlaybackBox;
class ProgramInfo;

class PlaybackBoxListItem : public MythUIButtonListItem
{
  public:
    PlaybackBoxListItem(PlaybackBox *parent, MythUIButtonList *lbtype, ProgramInfo *pi);

//    virtual void SetToRealButton(MythUIStateType *button, bool selected);

  private:
    PlaybackBox *pbbox;
    bool         needs_update;
};

#endif // _PLAYBACKBOXLISTITEM_H_
