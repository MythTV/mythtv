#ifndef MYTHRECT_H_
#define MYTHRECT_H_

#include <QRect>
#include <QString>

class MythRect : public QRect
{

  public:
    MythRect();
    MythRect(int x, int y, int width, int height);
    MythRect(QString sX, QString sY, QString sWidth, QString sHeight);
    MythRect(QRect rect);

    void Init();
    void CalculateArea(MythRect parentArea);

    void setRect(QString sX, QString sY, QString sWidth, QString sHeight);
    void setX(QString sX);
    void setY(QString sY);
    void setWidth(QString sWidth);
    void setHeight(QString sHeight);

    QRect toQRect();

  private:
    float m_percentWidth;
    float m_percentHeight;
    float m_percentX;
    float m_percentY;
};

#endif
