#ifndef MYTHPAINTER_QT_H_
#define MYTHPAINTER_QT_H_

#include <list>

#include "mythpainter.h"
#include "mythimage.h"

#include "compat.h"

class QPainter;

class MythQtPainter : public MythPainter
{
  public:
    MythQtPainter();
   ~MythQtPainter();

    virtual QString GetName(void)        { return QString("Qt"); }
    virtual bool SupportsAnimation(void) { return false; }
    virtual bool SupportsAlpha(void)     { return true; }
    virtual bool SupportsClipping(void)  { return true; }

    virtual void Begin(QPaintDevice *parent);
    virtual void End();

    virtual void SetClipRect(const QRect &clipRect);

    virtual void DrawImage(const QRect &dest, MythImage *im, const QRect &src,
                           int alpha);

  protected:
    virtual MythImage* GetFormatImagePriv(void);
    virtual void DeleteFormatImagePriv(MythImage *im);

    void DeletePixmaps(void);

    QPainter *painter;
    QRegion clipRegion;

    std::list<QPixmap *> m_imageDeleteList;
    QMutex               m_imageDeleteLock;
};

#endif
