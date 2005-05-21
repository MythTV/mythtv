#ifndef MYTHPAINTER_H_
#define MYTHPAINTER_H_

#include <qrect.h>
#include <qstring.h>
#include <qwidget.h>

//  #include "mythfontproperties.h"


class MythFontProperties;
class MythImage;

class MythPainter
{
  public:
    MythPainter() { }
    virtual ~MythPainter() { }

    virtual QString GetName(void) = 0;
    virtual bool SupportsAnimation(void) = 0;
    virtual bool SupportsAlpha(void) = 0;

    virtual void Begin(QWidget *parent) { m_Parent = parent; }
    virtual void End() { m_Parent = NULL; }

    QWidget *GetParent(void) { return m_Parent; }

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha) = 0;

    void DrawImage(int x, int y, MythImage *im, int alpha);
    void DrawImage(const QPoint &topLeft, MythImage *im, int alph);

    virtual void DrawText(const QRect &dest, const QString &msg, int flags,
                          const MythFontProperties &font, int alpha) = 0;

    virtual MythImage *GetFormatImage() = 0;

    // make friend so only callable from image
    virtual void DeleteFormatImage(MythImage *im) = 0;

  protected:
    QWidget *m_Parent;
};

#endif  
