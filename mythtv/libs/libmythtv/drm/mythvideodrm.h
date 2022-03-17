#ifndef MYTHVIDEODRM_H
#define MYTHVIDEODRM_H

// Qt
#include <QObject>

// MythTV
#include "libmythui/platforms/drm/mythdrmplane.h"
#include "drm/mythvideodrmbuffer.h"

class MythVideoFrame;
class MythVideoColourSpace;

class MythVideoDRM : public QObject
{
    Q_OBJECT

  public:
    explicit MythVideoDRM(MythVideoColourSpace* ColourSpace);
   ~MythVideoDRM() override;

    bool IsValid() const { return m_valid; };
    bool RenderFrame(AVDRMFrameDescriptor* DRMDesc, MythVideoFrame* Frame);

  public slots:
    void ColourSpaceUpdated(bool /*PrimariesChanged*/);

  private:
    bool         m_valid      { false   };
    MythDRMPtr   m_device     { nullptr };
    DRMPlane     m_videoPlane { nullptr };
    QRect        m_lastSrc;
    QRect        m_lastDst;
    QMap<AVDRMFrameDescriptor*,DRMHandle> m_handles;
    MythVideoColourSpace* m_colourSpace { nullptr };
};

#endif
