#include "playbackboxlistitem.h"
#include "programinfo.h"
#include "playbackbox.h"
#include "mythverbose.h"

PlaybackBoxListItem::PlaybackBoxListItem(
    PlaybackBox *parent, MythUIButtonList *lbtype, ProgramInfo *pi) :
    MythUIButtonListItem(lbtype, "", qVariantFromValue(pi)), pbbox(parent)
{
}

void PlaybackBoxListItem::SetToRealButton(
    MythUIStateType *button, bool selected)
{
    pbbox->UpdateProgramInfo(this, selected);
    MythUIButtonListItem::SetToRealButton(button, selected);
}
