#ifndef MYTHPAINTER_QT_H_
#define MYTHPAINTER_QT_H_

#include "mythpainter.h"
#include "mythimage.h"

class QPainter;
class QPixmap;

class MythQtPainter : public MythPainter
{
  public:
    MythQtPainter();
   ~MythQtPainter();

    virtual QString GetName(void) { return QString("Qt"); }
    virtual bool SupportsAnimation(void) { return false; }
    virtual bool SupportsAlpha(void) { return false; }
    virtual bool SupportsClipping(void) { return true; }

    virtual void Begin(QWidget *parent);
    virtual void End();

    virtual void SetClipRect(const QRect &clipRect);

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha);
    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha);

    virtual MythImage *GetFormatImage();
    virtual void DeleteFormatImage(MythImage *im);

  protected:

    QPainter *painter;
    QPainter *mainPainter;
    QPixmap *drawPixmap;
    QRegion clipRegion;
};

#endif
