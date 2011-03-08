#ifndef MYTHPAINTER_D3D9_H
#define MYTHPAINTER_D3D9_H

#include <QMap>

#include "mythpainter.h"
#include "mythimage.h"
#include "mythrender_d3d9.h"
#include "mythuiexp.h"

class MythRenderD3D9;

class MUI_PUBLIC MythD3D9Painter : public MythPainter
{
  public:
    MythD3D9Painter(MythRenderD3D9 *render = NULL);
   ~MythD3D9Painter();

    void         SetTarget(D3D9Image *target) { m_target = target;  }
    void         SetSwapControl(bool swap) { m_swap_control = swap; }
    virtual QString GetName(void)        { return QString("D3D9");  }
    virtual bool SupportsAnimation(void) { return true;             }
    virtual bool SupportsAlpha(void)     { return true;             }
    virtual bool SupportsClipping(void)  { return false;            }
    virtual void FreeResources(void);
    virtual void Begin(QPaintDevice *parent);
    virtual void End();

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha);
    virtual void DrawRect(const QRect &area, const QBrush &fillBrush,
                          const QPen &linePen, int alpha);

  protected:
    virtual MythImage* GetFormatImagePriv(void) { return new MythImage(this); }
    virtual void DeleteFormatImagePriv(MythImage *im);
    bool InitD3D9(QPaintDevice *parent);
    void Teardown(void);
    void ClearCache(void);
    void DeleteBitmaps(void);
    D3D9Image* GetImageFromCache(MythImage *im);

    MythRenderD3D9               *m_render;
    bool                          m_created_render;
    D3D9Image                    *m_target;
    bool                          m_swap_control;
    QMap<MythImage *, D3D9Image*> m_ImageBitmapMap;
    std::list<MythImage *>        m_ImageExpireList;
    std::list<D3D9Image*>         m_bitmapDeleteList;
    QMutex                        m_bitmapDeleteLock;
};

#endif // MYTHPAINTER_D3D9_H
