// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythbase/programinfo.h"

//  MythFrontend
#include "playbackbox.h"
#include "playbackboxlistitem.h"

#ifdef INCLUDE_UNFINISHED
PlaybackBoxListItem::PlaybackBoxListItem(
    PlaybackBox *parent, MythUIButtonList *lbtype, ProgramInfo *pi) :
    MythUIButtonListItem(lbtype, "", QVariant::fromValue(pi)),
    m_pbbox(parent)
{
}

void PlaybackBoxListItem::SetToRealButton(
    MythUIStateType *button, bool selected)
{
    if (m_needs_update)
    {
        m_pbbox->UpdateUIListItem(this, selected);
        m_needs_update = false;
    }
    MythUIButtonListItem::SetToRealButton(button, selected);
}
#else
PlaybackBoxListItem::PlaybackBoxListItem(
    [[maybe_unused]] PlaybackBox *parent,
    MythUIButtonList *lbtype, ProgramInfo *pi) :
    MythUIButtonListItem(lbtype, "", QVariant::fromValue(pi))
{
}
#endif
