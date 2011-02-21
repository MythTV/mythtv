#ifndef MYTHPAINTER_QIMAGE_H_
#define MYTHPAINTER_QIMAGE_H_

// C++ includes
#include <list>

// Qt includes
#include <QMap>

// MythTV includes
#include "mythpainter.h"
#include "mythimage.h"
#include "compat.h"

class QPainter;

class MUI_PUBLIC MythQImagePainter : public MythPainter
{
  public:
    MythQImagePainter();
   ~MythQImagePainter();

    virtual QString GetName(void)        { return QString("QImage"); }
    virtual bool SupportsAnimation(void) { return false;             }
    virtual bool SupportsAlpha(void)     { return true;              }
    virtual bool SupportsClipping(void)  { return true;              }

    virtual void Begin(QPaintDevice *parent);
    virtual void End();

    virtual void SetClipRect(const QRect &clipRect);
    virtual void SetClipRegion(const QRegion &region);
    virtual void Clear(QPaintDevice *device, const QRegion &region);

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha);
    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha,
                          const QRect &boundRect);
    virtual void DrawRect(const QRect &area,
                          bool drawFill, const QColor &fillColor, 
                          bool drawLine, int lineWidth,
                          const QColor &lineColor);
    virtual void DrawRoundRect(const QRect &area, int radius, 
                               bool drawFill, const QColor &fillColor, 
                               bool drawLine, int lineWidth,
                               const QColor &lineColor);

  protected:
    void       CheckPaintMode(const QRect &area);
    void       ExpireImages(uint max = 0);
    MythImage *GetImageFromString(const QString &msg, int flags, const QRect &r,
                                  const MythFontProperties &font);
    MythImage *GetImageFromRect(const QSize &size, int radius,
                                bool drawFill, const QColor &fillColor,
                                bool drawLine, int lineWidth,
                                const QColor &lineColor);

    QPainter *painter;
    QRegion   clipRegion;
    QRegion   paintedRegion;
    bool      copy;

    QMap<QString, MythImage *> m_StringToImageMap;
    std::list<QString>         m_StringExpireList;
};

#endif

