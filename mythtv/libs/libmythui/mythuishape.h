#ifndef MYTHUISHAPE_H_
#define MYTHUISHAPE_H_

// QT headers
#include <QColor>
#include <QPen>
#include <QBrush>
#include <QLinearGradient>

// Mythui headers
#include "mythuitype.h"

class MythImage;

/** \class MythUIShape
 *
 * \brief A widget for rendering primitive shapes and lines.
 *
 * \ingroup MythUI_Widgets
 */
class MUI_PUBLIC MythUIShape : public MythUIType
{
  public:
    MythUIShape(MythUIType *parent, const QString &name);

    void SetCropRect(int x, int y, int width, int height);
    void SetCropRect(const MythRect &rect);
    void SetFillBrush(QBrush fill);
    void SetLinePen(QPen pen);

  protected:
    void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                  int alphaMod, QRect clipRect) override; // MythUIType

    bool ParseElement(const QString &filename, QDomElement &element,
                      bool showWarnings) override; // MythUIType
    void CopyFrom(MythUIType *base) override; // MythUIType
    void CreateCopy(MythUIType *parent) override; // MythUIType

  private:
    QString        m_type         {"box"};
    QBrush         m_fillBrush    {Qt::NoBrush};
    QPen           m_linePen      {Qt::NoPen};
    int            m_cornerRadius {10};
    MythRect       m_cropRect     {0,0,0,0};

    friend class MythUIProgressBar;
    friend class MythUIEditBar;
    friend class SubtitleFormat;
};

#endif
