#ifndef MYTHPAINTER_VDPAU_H_
#define MYTHPAINTER_VDPAU_H_

#include <cstdint>

#include <QMap>

#include "mythpainter.h"
#include "mythimage.h"

class MythRenderVDPAU;

class MUI_PUBLIC MythVDPAUPainter : public MythPainter
{
    friend class VideoOutputVDPAU;
  public:
    explicit MythVDPAUPainter(MythRenderVDPAU *render = nullptr);
   ~MythVDPAUPainter();

    void SetTarget(uint target)    { m_target = target;     }
    void SetSwapControl(bool swap) { m_swap_control = swap; }

    QString GetName(void) override // MythPainter
        { return QString("VDPAU"); }
    bool SupportsAnimation(void) override // MythPainter
        { return true; }
    bool SupportsAlpha(void)     override // MythPainter
        { return true; }
    bool SupportsClipping(void)  override // MythPainter
        { return false; }
    void FreeResources(void) override; // MythPainter
    void Begin(QPaintDevice *parent) override; // MythPainter
    void End() override; // MythPainter

    void DrawImage(const QRect &r, MythImage *im, const QRect &src,
                   int alpha) override; // MythPainter

  protected:
    MythImage* GetFormatImagePriv(void) override // MythPainter
        { return new MythImage(this); }
    void DeleteFormatImagePriv(MythImage *im) override; // MythPainter
    void Teardown(void) override; // MythPainter

    bool InitVDPAU(QPaintDevice *parent);
    void ClearCache(void);
    void DeleteBitmaps(void);
    uint GetTextureFromCache(MythImage *im);

    MythRenderVDPAU            *m_render       {nullptr};
    uint                        m_target       {0};
    bool                        m_swap_control {true};

    QMap<MythImage *, uint32_t> m_ImageBitmapMap;
    std::list<MythImage *>      m_ImageExpireList;
    std::list<uint32_t>         m_bitmapDeleteList;
    QMutex                      m_bitmapDeleteLock;
};

#endif

