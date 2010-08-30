#ifndef VIDEOPOPUPS_H_
#define VIDEOPOPUPS_H_

#include <mythscreentype.h>

class VideoMetadata;

class MythUIButtonList;
class MythUIButtonListItem;

class CastDialog : public MythScreenType
{
    Q_OBJECT

  public:
    CastDialog(MythScreenStack *lparent, VideoMetadata *metadata);

    bool Create();

  private:
    VideoMetadata *m_metadata;
};

class PlotDialog : public MythScreenType
{
    Q_OBJECT

  public:
    PlotDialog(MythScreenStack *lparent, VideoMetadata *metadata);

    bool Create();

  private:
    VideoMetadata *m_metadata;
};

#endif
