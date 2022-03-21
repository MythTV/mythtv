#ifndef VIDEOPOPUPS_H_
#define VIDEOPOPUPS_H_

#include "libmythui/mythscreentype.h"

class VideoMetadata;

class MythUIButtonList;
class MythUIButtonListItem;

class CastDialog : public MythScreenType
{
    Q_OBJECT

  public:
    CastDialog(MythScreenStack *lparent, VideoMetadata *metadata)
        : MythScreenType(lparent, "videocastpopup"), m_metadata(metadata) {}

    bool Create() override; // MythScreenType

  private:
    VideoMetadata *m_metadata {nullptr};
};

class PlotDialog : public MythScreenType
{
    Q_OBJECT

  public:
    PlotDialog(MythScreenStack *lparent, VideoMetadata *metadata)
        : MythScreenType(lparent, "videoplotpopup"), m_metadata(metadata) {}

    bool Create() override; // MythScreenType

  private:
    VideoMetadata *m_metadata {nullptr};
};

#endif
