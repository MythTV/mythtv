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
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    virtual bool ParseElement(
        const QString &filename, QDomElement &element, bool showWarnings);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

  private:
    QString        m_type;
    QBrush         m_fillBrush;
    QPen           m_linePen;
    int            m_cornerRadius;
    MythRect       m_cropRect;

    friend class MythUIProgressBar;
    friend class MythUIEditBar;
};

#endif
