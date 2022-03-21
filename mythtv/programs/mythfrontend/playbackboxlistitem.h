// -*- Mode: c++ -*-
// vim:set sw=4 ts=4 expandtab:
#ifndef PLAYBACKBOXLISTITEM_H
#define PLAYBACKBOXLISTITEM_H


#include "libmythui/mythuibuttonlist.h"

class PlaybackBox;
class ProgramInfo;

class PlaybackBoxListItem : public MythUIButtonListItem
{
  public:
    PlaybackBoxListItem(PlaybackBox *parent, MythUIButtonList *lbtype, ProgramInfo *pi);

//  void SetToRealButton(MythUIStateType *button, bool selected) override; // MythUIButtonListItem

  private:
#ifdef INCLUDE_UNFINISHED
    PlaybackBox *m_pbbox        {nullptr};
    bool         m_needs_update {true};
#endif
};

#endif // PLAYBACKBOXLISTITEM_H
