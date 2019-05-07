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
    MythQImagePainter() : MythPainter() {}
   ~MythQImagePainter();

    QString GetName(void) override // MythPainter
        { return QString("QImage"); }
    bool SupportsAnimation(void) override // MythPainter
        { return false; }
    bool SupportsAlpha(void) override // MythPainter
        { return true; }
    bool SupportsClipping(void) override // MythPainter
        { return true; }

    void Begin(QPaintDevice *parent) override; // MythPainter
    void End() override; // MythPainter

    void SetClipRect(const QRect &clipRect) override; // MythPainter
    void SetClipRegion(const QRegion &region) override; // MythPainter
    void Clear(QPaintDevice *device, const QRegion &region) override; // MythPainter

    void DrawImage(const QRect &r, MythImage *im, const QRect &src,
                   int alpha) override; // MythPainter
    void DrawText(const QRect &r, const QString &msg, int flags,
                  const MythFontProperties &font, int alpha,
                  const QRect &boundRect) override; // MythPainter
    void DrawRect(const QRect &area, const QBrush &fillBrush,
                  const QPen &linePen, int alpha) override; // MythPainter
    void DrawRoundRect(const QRect &area, int cornerRadius,
                       const QBrush &fillBrush, const QPen &linePen,
                       int alpha) override; // MythPainter
    void DrawEllipse(const QRect &area, const QBrush &fillBrush,
                     const QPen &linePen, int alpha) override; // MythPainter

  protected:
    MythImage* GetFormatImagePriv(void) override // MythPainter
        { return new MythImage(this); }
    void DeleteFormatImagePriv(MythImage */*im*/) override {} // MythPainter

    void CheckPaintMode(const QRect &area);

    QPainter *m_painter       {nullptr};
    QRegion   m_clipRegion;
    QRegion   m_paintedRegion;
    bool      m_copy          {false};
};

#endif

