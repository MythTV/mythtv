#ifndef MYTHRECT_H_
#define MYTHRECT_H_

#include <QRect>
#include <QPoint>
#include <QString>

#include "mythexp.h"

class MythPoint;

/** \class MythRect
 *
 *  \brief Wrapper around QRect allowing us to handle percentage and other
 *         relative values for areas in mythui
 */
class MPUBLIC MythRect : public QRect
{

  public:
    MythRect();
    MythRect(int x, int y, int width, int height);
    MythRect(const QString &sX, const QString &sY, const QString &sWidth,
             const QString &sHeight);
    MythRect(QRect rect);
    bool operator== (const MythRect &other) const;

    void Init(void);
    void CalculateArea(MythRect parentArea);

    void NormRect(void);

    void setRect(const QString &sX, const QString &sY, const QString &sWidth,
                 const QString &sHeight);
    void setRect(int X, int Y, int w,int h) { QRect::setRect(X,Y,w,h); }
    void setX(const QString &sX);
    void setX(int X) { QRect::setX(X); }
    void setY(const QString &sY);
    void setY(int Y) { QRect::setY(Y); }
    void setWidth(const QString &sWidth);
    void setWidth(int width) { QRect::setWidth(width); }
    void setHeight(const QString &sHeight);
    void setHeight(int height) { QRect::setHeight(height); }

    QString getX(void) const;
    QString getY(void) const;
    QString getWidth(void) const;
    QString getHeight(void) const;

    MythPoint topLeft(void) const;
    void moveTopLeft(const MythPoint &point);
    void moveLeft(const QString &sX);
    void moveLeft(int X) { QRect::moveLeft(X); }
    void moveTop(const QString &sX);
    void moveTop(int Y) { QRect::moveTop(Y); }

    QRect toQRect(void) const;

  private:
    float m_percentWidth;
    float m_percentHeight;
    float m_percentX;
    float m_percentY;

    bool m_needsUpdate;

    QRect m_parentArea;
};

/** \class MythPoint
 *
 * \brief Wrapper around QPoint allowing us to handle percentage and other
 *        relative values for positioning in mythui
 */
class MPUBLIC MythPoint : public QPoint
{

  public:
    MythPoint();
    MythPoint(int x, int y);
    MythPoint(const QString &sX, const QString &sY);
    MythPoint(QPoint point);

    void Init(void);
    void CalculatePoint(MythRect parentArea);

    void NormPoint(void);

    void setX(const QString &sX);
    void setX(int X) { QPoint::setX(X); }
    void setY(const QString &sY);
    void setY(int Y) { QPoint::setY(Y); }

    QString getX(void) const;
    QString getY(void) const;

    QPoint toQPoint(void) const;

  private:
    float m_percentX;
    float m_percentY;

    bool m_needsUpdate;

    QRect m_parentArea;
};

#endif
