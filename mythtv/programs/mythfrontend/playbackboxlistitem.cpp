#include "playbackboxlistitem.h"
#include "programinfo.h"
#include "playbackbox.h"
#include "mythverbose.h"

PlaybackBoxListItem::PlaybackBoxListItem(
    PlaybackBox *parent, MythUIButtonList *lbtype, ProgramInfo *pi) :
    MythUIButtonListItem(lbtype, "", qVariantFromValue(pi)),
    pbbox(parent), needs_update(true)
{
}
/*
void PlaybackBoxListItem::SetToRealButton(
    MythUIStateType *button, bool selected)
{
    if (needs_update)
    {
        pbbox->UpdateUIListItem(this, selected);
        needs_update = false;
    }
    MythUIButtonListItem::SetToRealButton(button, selected);
}
*/
