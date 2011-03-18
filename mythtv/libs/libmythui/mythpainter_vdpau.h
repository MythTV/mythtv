#ifndef MYTHPAINTER_VDPAU_H_
#define MYTHPAINTER_VDPAU_H_

#include <stdint.h>

#include <QMap>

#include "mythpainter.h"
#include "mythimage.h"

class MythRenderVDPAU;

class MUI_PUBLIC MythVDPAUPainter : public MythPainter
{
  public:
    MythVDPAUPainter(MythRenderVDPAU *render = NULL);
   ~MythVDPAUPainter();

    void SetTarget(uint target)    { m_target = target;     }
    void SetSwapControl(bool swap) { m_swap_control = swap; }

    virtual QString GetName(void)        { return QString("VDPAU"); }
    virtual bool SupportsAnimation(void) { return true;             }
    virtual bool SupportsAlpha(void)     { return true;             }
    virtual bool SupportsClipping(void)  { return false;            }
    virtual void FreeResources(void);
    virtual void Begin(QPaintDevice *parent);
    virtual void End();

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha);

  protected:
    virtual MythImage* GetFormatImagePriv(void) { return new MythImage(this); }
    virtual void DeleteFormatImagePriv(MythImage *im);

    bool InitVDPAU(QPaintDevice *parent);
    void Teardown(void);
    void ClearCache(void);
    void DeleteBitmaps(void);
    uint GetTextureFromCache(MythImage *im);

    MythRenderVDPAU            *m_render;
    bool                        m_created_render;
    uint                        m_target;
    bool                        m_swap_control;

    QMap<MythImage *, uint32_t> m_ImageBitmapMap;
    std::list<MythImage *>      m_ImageExpireList;
    std::list<uint32_t>         m_bitmapDeleteList;
    QMutex                      m_bitmapDeleteLock;
};

#endif

