#ifndef MYTHPAINTER_YUVA_H_
#define MYTHPAINTER_YUVA_H_

#include "mythpainter_qimage.h"
#include "mythimage.h"
#include "compat.h"

class MythFontProperties;

class MUI_PUBLIC MythYUVAPainter : public MythQImagePainter
{
  public:
    MythYUVAPainter() : MythQImagePainter() { }
   ~MythYUVAPainter();

    QString GetName(void) { return QString("YUVA"); }

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
    MythFontProperties* GetConvertedFont(const MythFontProperties &font);

    QMap<QString, MythFontProperties*> m_convertedFonts;
    std::list<QString> m_expireList;
};

#endif
