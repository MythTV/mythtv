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
    friend class VideoOutputD3D;
  public:
    explicit MythD3D9Painter(MythRenderD3D9 *render = nullptr);
   ~MythD3D9Painter();

    void         SetTarget(D3D9Image *target) { m_target = target;  }
    void         SetSwapControl(bool swap) { m_swap_control = swap; }
    QString GetName(void) override // MythPainter
        { return QString("D3D9");  }
    bool SupportsAnimation(void) override // MythPainter
        { return true; }
    bool SupportsAlpha(void) override // MythPainter
        { return true; }
    bool SupportsClipping(void) override // MythPainter
        { return false; }
    void FreeResources(void) override; // MythPainter
    void Begin(QPaintDevice *parent) override; // MythPainter
    void End() override; // MythPainter

    void DrawImage(QRect dest, MythImage *im, QRect src,
                   int alpha) override; // MythPainter
    void DrawRect(QRect area, const QBrush &fillBrush,
                  const QPen &linePen, int alpha) override; // MythPainter

  protected:
    MythImage* GetFormatImagePriv(void) override // MythPainter
        { return new MythImage(this); }
    void DeleteFormatImagePriv(MythImage *im) override; // MythPainter
    void Teardown(void) override; // MythPainter

    bool InitD3D9(QPaintDevice *parent);
    void ClearCache(void);
    void DeleteBitmaps(void);
    D3D9Image* GetImageFromCache(MythImage *im);

    MythRenderD3D9               *m_render       {nullptr};
    D3D9Image                    *m_target       {nullptr};
    bool                          m_swap_control {true};
    QMap<MythImage *, D3D9Image*> m_ImageBitmapMap;
    std::list<MythImage *>        m_ImageExpireList;
    std::list<D3D9Image*>         m_bitmapDeleteList;
    QMutex                        m_bitmapDeleteLock;
};

#endif // MYTHPAINTER_D3D9_H
