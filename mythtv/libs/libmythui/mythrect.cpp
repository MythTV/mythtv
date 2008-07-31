
#include "mythrect.h"

MythRect::MythRect()
         : QRect()
{
    Init();
}

MythRect::MythRect(int x, int y, int width, int height)
         : QRect(x, y, width, height)
{
    Init();
}

MythRect::MythRect(QString sX, QString sY, QString sWidth, QString sHeight)
         : QRect()
{
    Init();
    setRect(sX,sY,sWidth,sHeight);
}

MythRect::MythRect(QRect rect)
          : QRect(rect)
{
    Init();
}

void MythRect::Init()
{
    m_percentWidth = m_percentHeight = m_percentX = m_percentY = 0.0;
}

void MythRect::CalculateArea(MythRect parentArea)
{
    if (!parentArea.isValid())
        return;

    int w = width();
    int h = height();
    int X = x();
    int Y = y();

    if (m_percentX > 0)
        X = m_percentX * (parentArea.width());

    if (m_percentY > 0)
        Y = m_percentY * (parentArea.height());

    if (m_percentWidth > 0)
        w = m_percentWidth * (parentArea.width() - X);

    if (m_percentHeight > 0)
        h = m_percentHeight * (parentArea.height() - Y);

    QRect::setRect(X,Y,w,h);
}

void MythRect::setRect(QString sX, QString sY, QString sWidth, QString sHeight)
{
    setX(sX);
    setY(sY);
    setWidth(sWidth);
    setHeight(sHeight);
}

void MythRect::setX(QString sX)
{
    if (sX.endsWith("%"))
    {
        sX.chop(1);
        m_percentX = (sX.toInt()) / 100;
    }
    else
        QRect::setX(sX.toInt());
}

void MythRect::setY(QString sY)
{
    if (sY.endsWith("%"))
    {
        sY.chop(1);
        m_percentY = (sY.toInt()) / 100;
    }
    else
        QRect::setY(sY.toInt());
}


void MythRect::setWidth(QString sWidth)
{
    if (sWidth.endsWith("%"))
    {
        sWidth.chop(1);
        m_percentWidth = (sWidth.toInt()) / 100;
    }
    else
        QRect::setWidth(sWidth.toInt());
}

void MythRect::setHeight(QString sHeight)
{
    if (sHeight.endsWith("%"))
    {
        sHeight.chop(1);
        m_percentHeight = (sHeight.toInt()) / 100;
    }
    else
        QRect::setHeight(sHeight.toInt());
}

QRect MythRect::toQRect()
{
    return QRect(x(),y(),width(),height());
}

