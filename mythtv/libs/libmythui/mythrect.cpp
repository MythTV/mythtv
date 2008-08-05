
#include "mythmainwindow.h"
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

MythRect::MythRect(const MythRect &rect)
          : QRect(rect)
{
    Init();

    m_percentWidth = rect.m_percentWidth;
    m_percentHeight = rect.m_percentHeight;
    m_percentX = rect.m_percentX;
    m_percentY = rect.m_percentY;

    m_needsUpdate = rect.m_needsUpdate;
}

MythRect& MythRect::operator=(const MythRect& rect)
{
    if (this != &rect)
    {
        QRect::operator=(rect);
        m_percentWidth = rect.m_percentWidth;
        m_percentHeight = rect.m_percentHeight;
        m_percentX = rect.m_percentX;
        m_percentY = rect.m_percentY;
        m_needsUpdate = rect.m_needsUpdate;
    }
    return *this;
}

void MythRect::Init()
{
    m_needsUpdate = true;
    m_percentWidth = m_percentHeight = m_percentX = m_percentY = 0.0;
}

void MythRect::CalculateArea(MythRect parentArea)
{
    QRect area  = parentArea.toQRect();
    if ((m_parentArea == area && !m_needsUpdate) || !parentArea.isValid())
        return;

    m_parentArea  = area;

    int w = width();
    int h = height();
    int X = x();
    int Y = y();

    if (m_percentX > 0.0)
        X = m_percentX * (float)(m_parentArea.width());
    if (m_percentY > 0.0)
        Y = m_percentY * (float)(m_parentArea.height());
    if (m_percentWidth > 0.0)
        w = m_percentWidth * (float)(m_parentArea.width() - X);
    if (m_percentHeight > 0.0)
        h = m_percentHeight * (float)(m_parentArea.height() - Y);

    QRect::setRect(X,Y,w,h);

    m_needsUpdate = false;
}

void MythRect::NormRect(void)
{

    if (m_percentWidth == 0.0)
        QRect::setWidth(GetMythMainWindow()->NormX(width()));

    if (m_percentHeight == 0.0)
        QRect::setHeight(GetMythMainWindow()->NormY(height()));

    int X = 0;
    if (m_percentX == 0.0)
        X = GetMythMainWindow()->NormX(x());

    int Y = 0;
    if (m_percentY == 0.0)
        Y = GetMythMainWindow()->NormY(y());

    moveTopLeft(QPoint(X,Y));

    normalized();
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
        m_percentX = X.toFloat() / 100.0;
        m_needsUpdate = true;
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
        m_percentY = Y.toFloat() / 100.0;
        m_needsUpdate = true;
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
        m_percentWidth = width.toFloat() / 100.0;
        m_needsUpdate = true;
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
        m_percentHeight = height.toFloat() / 100.0;
        m_needsUpdate = true;
    }
    else
        QRect::setHeight(height.toInt());
}

QString MythRect::getX(void)
{
    QString stringX;
    if (m_percentX > 0.0)
        stringX = QString("%1%").arg((int)(m_percentX * 100));
    else
        stringX = QString("%1").arg(x());
    return stringX;
}

QString MythRect::getY(void)
{
    QString stringY;
    if (m_percentY > 0.0)
        stringY = QString("%1%").arg((int)(m_percentY * 100));
    else
        stringY = QString("%1").arg(y());
    return stringY;
}

QString MythRect::getWidth(void)
{
    QString stringWidth;
    if (m_percentWidth > 0.0)
        stringWidth = QString("%1%").arg((int)(m_percentWidth * 100));
    else
        stringWidth = QString("%1").arg(width());
    return stringWidth;
}

QString MythRect::getHeight(void)
{
    QString stringHeight;
    if (m_percentHeight > 0.0)
        stringHeight = QString("%1%").arg((int)(m_percentHeight * 100));
    else
        stringHeight = QString("%1").arg(height());
    return stringHeight;
}

QRect MythRect::toQRect()
{
    return QRect(x(),y(),width(),height());
}

