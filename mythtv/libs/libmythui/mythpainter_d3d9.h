#ifndef MYTHPAINTER_D3D9_H
#define MYTHPAINTER_D3D9_H

#include <QMap>

#include "mythpainter.h"
#include "mythimage.h"
#include "mythrender_d3d9.h"

class MythRenderD3D9;

class MythD3D9Painter : public MythPainter
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
    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha,
                          const QRect &boundRect);
    virtual void DrawRect(const QRect &area, bool drawFill,
                          const QColor &fillColor, bool drawLine, int lineWidth,
                          const QColor &lineColor);
    virtual void DrawRoundRect(const QRect &area, int radius, bool drawFill,
                               const QColor &fillColor, bool drawLine,
                               int lineWidth, const QColor &lineColor);

  protected:
    virtual void DeleteFormatImagePriv(MythImage *im);
    bool InitD3D9(QPaintDevice *parent);
    void Teardown(void);
    void ClearCache(void);
    void DeleteBitmaps(void);
    D3D9Image* GetImageFromCache(MythImage *im);
    MythImage *GetImageFromString(const QString &msg, int flags, const QRect &r,
                                  const MythFontProperties &font);
    MythImage *GetImageFromRect(const QSize &size, int radius,
                                bool drawFill, const QColor &fillColor,
                                bool drawLine, int lineWidth,
                                const QColor &lineColor);
    void ExpireImages(uint max = 0);

    MythRenderD3D9               *m_render;
    bool                          m_created_render;
    D3D9Image                    *m_target;
    bool                          m_swap_control;
    QMap<MythImage *, D3D9Image*> m_ImageBitmapMap;
    std::list<MythImage *>        m_ImageExpireList;
    QMap<QString, MythImage *>    m_StringToImageMap;
    std::list<QString>            m_StringExpireList;
    std::list<D3D9Image*>         m_bitmapDeleteList;
    QMutex                        m_bitmapDeleteLock;
};

#endif // MYTHPAINTER_D3D9_H
