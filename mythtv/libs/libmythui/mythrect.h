#ifndef MYTHRECT_H_
#define MYTHRECT_H_

#include <QRect>
#include <QString>

class MythRect : public QRect
{

  public:
    MythRect();
    MythRect(int x, int y, int width, int height);
    MythRect(const QString &sX, const QString &sY, const QString &sWidth,
             const QString &sHeight);
    MythRect(QRect rect);

    void Init(void);
    void CalculateArea(MythRect parentArea);

    void setRect(const QString &sX, const QString &sY, const QString &sWidth,
                 const QString &sHeight);
    void setX(const QString &sX);
    void setY(const QString &sY);
    void setWidth(const QString &sWidth);
    void setHeight(const QString &sHeight);

    QRect toQRect(void);

  private:
    float m_percentWidth;
    float m_percentHeight;
    float m_percentX;
    float m_percentY;
};

#endif
