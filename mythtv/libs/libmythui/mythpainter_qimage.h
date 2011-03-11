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
    virtual void DrawRect(const QRect &area, const QBrush &fillBrush,
                          const QPen &linePen, int alpha);
    virtual void DrawRoundRect(const QRect &area, int cornerRadius,
                               const QBrush &fillBrush, const QPen &linePen,
                               int alpha);
    virtual void DrawEllipse(const QRect &area, const QBrush &fillBrush,
                             const QPen &linePen, int alpha);

  protected:
    virtual MythImage* GetFormatImagePriv(void) { return new MythImage(this); }
    virtual void DeleteFormatImagePriv(MythImage *im) { }

    void CheckPaintMode(const QRect &area);

    QPainter *painter;
    QRegion   clipRegion;
    QRegion   paintedRegion;
    bool      copy;
};

#endif

