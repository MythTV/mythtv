#ifndef MYTHPAINTER_VDPAU_H_
#define MYTHPAINTER_VDPAU_H_

#include <QMap>

#include "mythpainter.h"
#include "mythimage.h"

class MythVDPAUPrivate;

class MythVDPAUPainter : public MythPainter
{
  public:
    MythVDPAUPainter();
   ~MythVDPAUPainter();

    virtual QString GetName(void) { return QString("VDPAU"); }
    virtual bool SupportsAnimation(void) { return true; }
    virtual bool SupportsAlpha(void) { return true; }
    virtual bool SupportsClipping(void) { return false; }

    virtual void Begin(QWidget *parent);
    virtual void End();

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha);
    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha,
                          const QRect &boundRect);
    virtual void DrawRect(const QRect &area,
                          bool drawFill, const QColor &fillColor,
                          bool drawLine, int lineWidth, const QColor &lineColor);
    virtual void DrawRoundRect(const QRect &area, int radius,
                               bool drawFill, const QColor &fillColor,
                               bool drawLine, int lineWidth, const QColor &lineColor);

    virtual MythImage *GetFormatImage();
    virtual void DeleteFormatImage(MythImage *im);

  protected:
    MythVDPAUPrivate *d;


};

#endif
