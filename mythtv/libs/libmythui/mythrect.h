#ifndef MYTHRECT_H_
#define MYTHRECT_H_

#include <QRect>
#include <QString>

class MythRect;

class MythRect : public QRect
{

  public:
    MythRect();
    MythRect(int x, int y, int width, int height);
    MythRect(const QString &sX, const QString &sY, const QString &sWidth,
             const QString &sHeight);
    MythRect(QRect rect);

    MythRect(const MythRect &rect);
    MythRect& operator=(const MythRect& rect);

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

    QString getX(void);
    QString getY(void);
    QString getWidth(void);
    QString getHeight(void);

    QRect toQRect(void);

  private:
    float m_percentWidth;
    float m_percentHeight;
    float m_percentX;
    float m_percentY;

    bool m_needsUpdate;

    QRect m_parentArea;
};

#endif
