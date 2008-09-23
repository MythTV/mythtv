#ifndef VIDEOTREE_H_
#define VIDEOTREE_H_

#include <mythtv/libmythui/mythscreentype.h>
#include <mythtv/libmythui/mythuibuttontree.h>

#include "videodlg.h"

class Metadata;
class VideoList;
class ParentalLevel;

class VideoTree : public VideoDialog
{
    Q_OBJECT

  public:
    VideoTree(MythScreenStack *parent, const QString &name,
              VideoList *video_list, DialogType type=DLG_TREE);
   ~VideoTree();

    bool Create(void);

  private:
    void loadData(void);
    MythUIButtonListItem* GetItemCurrent(void);

    MythUIButtonTree* m_videoButtonTree;
    bool m_rememberPosition;
};

#endif
