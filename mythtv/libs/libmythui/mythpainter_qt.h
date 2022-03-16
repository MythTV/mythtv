#ifndef MYTHPAINTER_QT_H_
#define MYTHPAINTER_QT_H_

#include <list>

#include "mythpainter.h"
#include "mythimage.h"

#include "libmythbase/compat.h"

class QPainter;

class MythQtPainter : public MythPainter
{
  public:
    MythQtPainter() = default;
   ~MythQtPainter() override;

    QString GetName(void) override // MythPainter
        { return QString("Qt"); }
    bool SupportsAnimation(void) override // MythPainter
        { return false; }
    bool SupportsAlpha(void) override // MythPainter
        { return true; }
    bool SupportsClipping(void) override // MythPainter
        { return true; }

    void Begin(QPaintDevice *parent) override; // MythPainter
    void End() override; // MythPainter

    void SetClipRect(QRect clipRect) override; // MythPainter

    void DrawImage(QRect r, MythImage *im, QRect src,
                   int alpha) override; // MythPainter

  protected:
    MythImage* GetFormatImagePriv(void) override; // MythPainter
    void DeleteFormatImagePriv(MythImage *im) override; // MythPainter

    void DeletePixmaps(void);

    QPainter *m_painter    {nullptr};
    QRegion   m_clipRegion;

    std::list<QPixmap *> m_imageDeleteList;
    QMutex               m_imageDeleteLock;
};

#endif
