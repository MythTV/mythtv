
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

MythRect::MythRect(const QString &sX, const QString &sY, const QString &sWidth,
                   const QString &sHeight)
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

void MythRect::setRect(const QString &sX, const QString &sY, const QString &sWidth,
                       const QString &sHeight)
{
    setX(sX);
    setY(sY);
    setWidth(sWidth);
    setHeight(sHeight);
}

void MythRect::setX(const QString &sX)
{
    QString X = sX;
    if (X.endsWith("%"))
    {
        X.chop(1);
        m_percentX = (X.toInt()) / 100;
    }
    else
        QRect::setX(X.toInt());
}

void MythRect::setY(const QString &sY)
{
    QString Y = sY;
    if (Y.endsWith("%"))
    {
        Y.chop(1);
        m_percentY = (Y.toInt()) / 100;
    }
    else
        QRect::setY(Y.toInt());
}


void MythRect::setWidth(const QString &sWidth)
{
    QString width = sWidth;
    if (width.endsWith("%"))
    {
        width.chop(1);
        m_percentWidth = (width.toInt()) / 100;
    }
    else
        QRect::setWidth(width.toInt());
}

void MythRect::setHeight(const QString &sHeight)
{
    QString height = sHeight;
    if (height.endsWith("%"))
    {
        height.chop(1);
        m_percentHeight = (height.toInt()) / 100;
    }
    else
        QRect::setHeight(height.toInt());
}

QRect MythRect::toQRect()
{
    return QRect(x(),y(),width(),height());
}

