#ifndef MYTHUISHAPE_H_
#define MYTHUISHAPE_H_

// QT headers
#include <QColor>

// Mythui headers
#include "mythuitype.h"

class MPUBLIC MythUIShape : public MythUIType
{
  public:
    MythUIShape(MythUIType *parent, const QString &name);
    ~MythUIShape();

    void Reset(void);

  protected:
    virtual void DrawSelf(MythPainter *p, int xoffset, int yoffset,
                          int alphaMod, QRect clipRect);

    virtual bool ParseElement(QDomElement &element);
    virtual void CopyFrom(MythUIType *base);
    virtual void CreateCopy(MythUIType *parent);

  private:
    QString m_type;
    bool    m_drawFill;
    bool    m_drawLine;
    QColor  m_fillColor;
    QColor  m_lineColor;
    int     m_lineWidth;
    int     m_cornerRadius;
};

#endif
