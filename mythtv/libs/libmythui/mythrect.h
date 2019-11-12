#ifndef MYTHRECT_H_
#define MYTHRECT_H_

#include <QRect>
#include <QPoint>
#include <QString>

#include "mythuiexp.h"

class MythPoint;

/** \class MythRect
 *
 *  \brief Wrapper around QRect allowing us to handle percentage and other
 *         relative values for areas in mythui
 */
class MUI_PUBLIC MythRect : public QRect
{

  public:
    MythRect()
        : QRect() {}
    MythRect(int x, int y, int width, int height)
        : QRect(x, y, width, height) {}
    MythRect(const QString &sX, const QString &sY, const QString &sWidth,
             const QString &sHeight, const QString &baseRes = QString());
    MythRect(QRect rect)
        : QRect(rect) {}
    bool operator== (const MythRect &other) const;

    void Reset(void);
    void CalculateArea(const MythRect & parentArea);

    void NormRect(void);

    void setRect(const QString &sX, const QString &sY, const QString &sWidth,
                 const QString &sHeight, const QString &baseRes = QString());
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
    void moveTop(const QString &sY);
    void moveTop(int Y) { QRect::moveTop(Y); }

    QString toString(bool details = false) const;
    QRect toQRect(void) const;

  private:
    static bool parsePosition(float &percent, int &offset, int &absolute,
                              const QString &value, bool is_size);

    float m_percentWidth  {0.0F};
    float m_percentHeight {0.0F};
    float m_percentX      {0.0F};
    float m_percentY      {0.0F};
    int   m_offsetWidth   {0};
    int   m_offsetHeight  {0};
    int   m_offsetX       {0};
    int   m_offsetY       {0};

    bool  m_needsUpdate   {true};

    QRect m_parentArea    {0,0,0,0};
};

/** \class MythPoint
 *
 * \brief Wrapper around QPoint allowing us to handle percentage and other
 *        relative values for positioning in mythui
 */
class MUI_PUBLIC MythPoint : public QPoint
{

  public:
    MythPoint()
        : QPoint(), m_valid(false) {};
    MythPoint(int x, int y)
        : QPoint(x, y) {}
    MythPoint(const QString &sX, const QString &sY);
    MythPoint(QPoint point)
        : QPoint(point) {}

    bool isValid(void) const { return m_valid; }
    void CalculatePoint(const MythRect & parentArea);

    void NormPoint(void);

    void setX(const QString &sX);
    void setX(int X) { QPoint::setX(X); }
    void setY(const QString &sY);
    void setY(int Y) { QPoint::setY(Y); }

    QString getX(void) const;
    QString getY(void) const;

    QString toString(bool details = false) const;
    QPoint toQPoint(void) const;

  private:
    static bool parsePosition(float &percent, int &offset, int &absolute,
                              const QString &value);

    float m_percentX    {0.0F};
    float m_percentY    {0.0F};
    int   m_offsetX     {0};
    int   m_offsetY     {0};

    bool  m_needsUpdate {true};

    QRect m_parentArea;

    bool  m_valid       {true};
};

#endif
