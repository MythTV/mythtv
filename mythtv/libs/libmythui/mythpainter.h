#ifndef MYTHPAINTER_H_
#define MYTHPAINTER_H_

#include <QString>
#include <QWidget>
#include <QPaintDevice>
#include <QMutex>

class QRect;
class QRegion;
class QPoint;
class QColor;

//  #include "mythfontproperties.h"

#include "compat.h"

#include "mythexp.h"

class MythFontProperties;
class MythImage;

class MPUBLIC MythPainter
{
  public:
    MythPainter() : m_Parent(0), m_CacheSize(0) { }
    virtual ~MythPainter();

    virtual QString GetName(void) = 0;
    virtual bool SupportsAnimation(void) = 0;
    virtual bool SupportsAlpha(void) = 0;
    virtual bool SupportsClipping(void) = 0;
    virtual void FreeResources(void) { }
    virtual void Begin(QPaintDevice *parent) { m_Parent = parent; }
    virtual void End() { m_Parent = NULL; }

    virtual void SetClipRect(const QRect &clipRect);
    virtual void SetClipRegion(const QRegion &clipRegion);
    virtual void Clear(QPaintDevice *device, const QRegion &region);

    QPaintDevice *GetParent(void) { return m_Parent; }

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha) = 0;

    void DrawImage(int x, int y, MythImage *im, int alpha);
    void DrawImage(const QPoint &topLeft, MythImage *im, int alph);

    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha,
                          const QRect &boundRect) = 0;

    virtual void DrawRect(const QRect &area,
                          bool drawFill, const QColor &fillColor,
                          bool drawLine, int lineWidth, const QColor &lineColor) = 0;
    virtual void DrawRoundRect(const QRect &area, int radius,
                               bool drawFill, const QColor &fillColor,
                               bool drawLine, int lineWidth, const QColor &lineColor) = 0;

    virtual MythImage *GetFormatImage();

    // make friend so only callable from image
    virtual void DeleteFormatImage(MythImage *im);

  protected:
    virtual void DeleteFormatImagePriv(MythImage *im) { (void) im; }
    void CheckFormatImage(MythImage *im);
    void IncreaseCacheSize(QSize size);
    void DecreaseCacheSize(QSize size);

    QPaintDevice     *m_Parent;
    int               m_CacheSize;
    static int        m_MaxCacheSize;
    QList<MythImage*> m_allocatedImages;
    QMutex            m_allocationLock;
};

#endif
