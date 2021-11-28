
#include <QTextStream>

#include "mythrect.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"

MythRect::MythRect(const QString &sX, const QString &sY, const QString &sWidth,
                   const QString &sHeight, const QString &baseRes)
{
    setRect(sX,sY,sWidth,sHeight,baseRes);
}

/*
 * An explicit assignment operator from QRect to MythRect.  This
 * takes the place of an implicitly converting from a QRect to a
 * MythRect and then calling the compiler generated default
 * assignment operator.
 */
MythRect& MythRect::operator= (const QRect other)
{
    if (this == &other)
        return *this;

    // Set coords from the point.
    QRect::setX(other.x());
    QRect::setY(other.y());
    QRect::setWidth(other.width());
    QRect::setHeight(other.height());

    // Restore everything else to default.
    m_percentWidth  = 0.0F;
    m_percentHeight = 0.0F;
    m_percentX      = 0.0F;
    m_percentY      = 0.0F;
    m_offsetWidth   = 0;
    m_offsetHeight  = 0;
    m_offsetX       = 0;
    m_offsetY       = 0;
    m_needsUpdate   = true;
    m_parentArea    = {0,0,0,0};

    return *this;
};

bool MythRect::operator== (const MythRect &other) const
{
    return ((m_percentWidth == other.m_percentWidth) &&
            (m_percentHeight == other.m_percentHeight) &&
            (m_percentX == other.m_percentX) &&
            (m_percentY == other.m_percentY) &&
            (m_offsetWidth == other.m_offsetWidth) &&
            (m_offsetHeight == other.m_offsetHeight) &&
            (m_offsetX == other.m_offsetX) &&
            (m_offsetY == other.m_offsetY) &&
            (QRect)(*this) == (QRect)other);
}

void MythRect::Reset(void)
{
    m_parentArea.setRect(0, 0, 0, 0);
}

void MythRect::CalculateArea(const QRect parentArea)
{
    if ((m_parentArea == parentArea && !m_needsUpdate) || !parentArea.isValid())
        return;

    m_parentArea = parentArea;

    int w = width();
    int h = height();
    int X = x();
    int Y = y();

    if (m_percentX > 0.0F)
        X = (int) (m_percentX * (float)m_parentArea.width()) + m_offsetX;
    if (m_percentY > 0.0F)
        Y = (int) (m_percentY * (float)m_parentArea.height()) + m_offsetY;
    if (m_percentWidth > 0.0F)
    {
        w = (int) (m_percentWidth * (float)(m_parentArea.width() - X))
            + m_offsetWidth;
    }
    else if (m_offsetWidth != 0)
    {
        w = m_parentArea.width() - X + m_offsetWidth;
    }
    if (m_percentHeight > 0.0F)
    {
        h = (int) (m_percentHeight * (float)(m_parentArea.height() - Y))
            + m_offsetHeight;
    }
    else if (m_offsetHeight != 0)
    {
        h = m_parentArea.height() - Y + m_offsetHeight;
    }

    QRect::setRect(X,Y,w,h);

    m_needsUpdate = false;
}

void MythRect::CalculateArea(const MythRect & parentArea)
{
    CalculateArea(parentArea.toQRect());
}

void MythRect::NormRect(void)
{

    if (m_percentWidth == 0.0F)
        QRect::setWidth(GetMythMainWindow()->NormX(width()));

    if (m_percentHeight == 0.0F)
        QRect::setHeight(GetMythMainWindow()->NormY(height()));

    int X = 0;
    if (m_percentX == 0.0F)
        X = GetMythMainWindow()->NormX(x());

    int Y = 0;
    if (m_percentY == 0.0F)
        Y = GetMythMainWindow()->NormY(y());

    m_offsetX = GetMythMainWindow()->NormX(m_offsetX);
    m_offsetY = GetMythMainWindow()->NormY(m_offsetY);
    m_offsetWidth = GetMythMainWindow()->NormX(m_offsetWidth);
    m_offsetHeight = GetMythMainWindow()->NormY(m_offsetHeight);

    QRect::moveTopLeft(QPoint(X,Y));

    //  This would apply the result of normalized to the rect but causes
    //  problems with theme display
    //    QRect tr = normalized();
    //    setRect(tr.x(), tr.y(), tr.width(), tr.height());
}

void MythRect::setRect(const QString &sX, const QString &sY,
                       const QString &sWidth, const QString &sHeight,
                       const QString &rectBaseRes)
{

    // cater for an extra paramater on area and similar tags
    // for base resolution.

    QString vX = sX;
    QString vY = sY;
    QString vWidth = sWidth;
    QString vHeight = sHeight;
    if (!rectBaseRes.isEmpty())
    {
        QStringList res = rectBaseRes.split('x');
        if (res.size() == 2)
        {
            QSize themeBaseSize = GetMythUI()->GetBaseSize();
            int rectBaseWidth = res[0].toInt();
            int rectBaseHeight = res[1].toInt();
            if (rectBaseWidth > 0 && rectBaseHeight > 0)
            {
                int iX = sX.toInt();
                if (iX > 0)
                {
                    iX = iX * themeBaseSize.width() / rectBaseWidth;
                    vX = QString::number(iX);
                }
                int iY = sY.toInt();
                if (iY > 0)
                {
                    iY = iY * themeBaseSize.height() / rectBaseHeight;
                    vY = QString::number(iY);
                }
                int iWidth = sWidth.toInt();
                if (iWidth > 0)
                {
                    iWidth = iWidth * themeBaseSize.width() / rectBaseWidth;
                    vWidth = QString::number(iWidth);
                }
                int iHeight = sHeight.toInt();
                if (iHeight > 0)
                {
                    iHeight = iHeight * themeBaseSize.height() / rectBaseHeight;
                    vHeight = QString::number(iHeight);
                }
            }
        }
    }
    setX(vX);
    setY(vY);
    setWidth(vWidth);
    setHeight(vHeight);
}

/**
 * \brief parse the position
 *
 * \return true of position is absolute
 */
bool MythRect::parsePosition(float & percent, int & offset, int & absolute,
                             const QString &value, bool is_size)
{
    /*
      Position can be either an absolute, or a percentage with an
      optional offset.

      720
      -5
      33%
      75%+10
     */

    percent = 0.0;
    offset = 0;
    absolute = 0;

    if (value.isEmpty())
        return true;

    int  number = 0;
    char ch = ' ';
    QString tmp(value); // QTextStream won't accept a const!
    QTextStream is(&tmp);

    is >> number;
    if (is.status() != QTextStream::Ok)
        return true;

    is.skipWhiteSpace();
    is >> ch;
    if (ch != '%')
    {
        if (is_size && number < 0)
        {
            offset = number;
            return false;
        }
        absolute = number;
        return true;
    }

    percent = static_cast<float>(number) / 100.0F;
    is >> offset;
    return false;
}

void MythRect::setX(const QString &sX)
{
    int absoluteX = 0;

    if (parsePosition(m_percentX, m_offsetX, absoluteX, sX, false))
        QRect::setX(absoluteX);
    else
        m_needsUpdate = true;
}

void MythRect::setY(const QString &sY)
{
    int absoluteY = 0;

    if (parsePosition(m_percentY, m_offsetY, absoluteY, sY, false))
        QRect::setY(absoluteY);
    else
        m_needsUpdate = true;
}

void MythRect::setWidth(const QString &sWidth)
{
    int absoluteWidth = 0;

    if (parsePosition(m_percentWidth, m_offsetWidth, absoluteWidth,
                      sWidth, true))
        QRect::setWidth(absoluteWidth);
    else
        m_needsUpdate = true;
}

void MythRect::setHeight(const QString &sHeight)
{
    int absoluteHeight = 0;

    if (parsePosition(m_percentHeight, m_offsetHeight, absoluteHeight,
                      sHeight, true))
        QRect::setHeight(absoluteHeight);
    else
        m_needsUpdate = true;
}

MythPoint MythRect::topLeft(void) const
{
    MythPoint point;
    point.setX(getX());
    point.setY(getY());
    return point;
}

void MythRect::moveTopLeft(const QPoint point)
{
    // No need to convert coordinates to strings and back since there
    // is no percent scaling on a raw QPoint.
    QRect::moveTopLeft(point);
}

void MythRect::moveTopLeft(const MythPoint &point)
{
    moveLeft(point.getX());
    moveTop(point.getY());
}

void MythRect::moveLeft(const QString &sX)
{
    int absoluteX = 0;

    if (parsePosition(m_percentX, m_offsetX, absoluteX, sX, false))
        QRect::moveLeft(absoluteX);
    else // Move left to the absolute pos specified by the percentage/offset
        m_needsUpdate = true;
}

void MythRect::moveTop(const QString &sY)
{
    int absoluteY = 0;

    if (parsePosition(m_percentY, m_offsetY, absoluteY, sY, false))
        QRect::moveTop(absoluteY);
    else  // Move top to the absolute pos specified by the percentage/offset
        m_needsUpdate = true;
}

QString MythRect::getX(void) const
{
    QString stringX;
    if (m_percentX > 0.0F)
        stringX = QString("%1%").arg((int)(m_percentX * 100));
    else
        stringX = QString("%1").arg(x() - m_offsetX);
    if (m_offsetX != 0)
    {
        if (m_offsetX > 0)
            stringX += '+';
        stringX += QString("%1").arg(m_offsetX);
    }
    return stringX;
}

QString MythRect::getY(void) const
{
    QString stringY;
    if (m_percentY > 0.0F)
        stringY = QString("%1%").arg((int)(m_percentY * 100));
    else
        stringY = QString("%1").arg(y() - m_offsetY);
    if (m_offsetY != 0)
    {
        if (m_offsetY > 0)
            stringY += '+';
        stringY += QString("%1").arg(m_offsetY);
    }
    return stringY;
}

QString MythRect::getWidth(void) const
{
    QString stringWidth;
    if (m_percentWidth > 0.0F)
        stringWidth = QString("%1%").arg((int)(m_percentWidth * 100));
    else
        stringWidth = QString("%1").arg(width() - m_offsetWidth);
    if (m_offsetWidth != 0)
    {
        if (m_offsetWidth > 0)
            stringWidth += '+';
        stringWidth += QString("%1").arg(m_offsetWidth);
    }
    return stringWidth;
}

QString MythRect::getHeight(void) const
{
    QString stringHeight;
    if (m_percentHeight > 0.0F)
        stringHeight = QString("%1%").arg((int)(m_percentHeight * 100));
    else
        stringHeight = QString("%1").arg(height() - m_offsetHeight);
    if (m_offsetHeight != 0)
    {
        if (m_offsetHeight > 0)
            stringHeight += '+';
        stringHeight += QString("%1").arg(m_offsetHeight);
    }
    return stringHeight;
}

QString MythRect::toString(bool details) const
{
    QString result = QString("(%1,%2,%3,%4)")
                     .arg(x()).arg(y()).arg(width()).arg(height());

    if (details)
        result += QString(" [%1,%2,%3,%4]")
                  .arg(getX(), getY(), getWidth(), getHeight());

    return result;
}

QRect MythRect::toQRect() const
{
    return {x(),y(),width(),height()};
}

///////////////////////////////////////////////////////////////////

MythPoint::MythPoint(const QString &sX, const QString &sY)
{
    setX(sX);
    setY(sY);
}

/*
 * An explicit assignment operator from QPoint to MythPoint.  This
 * takes the place of an implicitly converting from a QPoint to a
 * MythPoint and then calling the compiler generated default
 * assignment operator.
 */
MythPoint& MythPoint::operator= (const QPoint other)
{
    if (this == &other)
        return *this;

    // Set coords from the point.
    QPoint::setX(other.x());
    QPoint::setY(other.y());

    // Restore everything else to default.
    m_percentX    = 0.0F;
    m_percentY    = 0.0F;
    m_offsetX     = 0;
    m_offsetY     = 0;
    m_needsUpdate = true;
    m_parentArea  = {};
    m_valid       = true;
    return *this;
};

void MythPoint::CalculatePoint(const QRect parentArea)
{
    if ((m_parentArea == parentArea && !m_needsUpdate) || !parentArea.isValid())
        return;

    m_parentArea  = parentArea;

    int X = x();
    int Y = y();

    if (m_percentX > 0.0F)
        X = (int) (m_percentX * (float)m_parentArea.width()) + m_offsetX;
    if (m_percentY > 0.0F)
        Y = (int) (m_percentY * (float)m_parentArea.height()) + m_offsetY;

    QPoint::setX(X);
    QPoint::setY(Y);

    m_needsUpdate = false;
    m_valid = true;
}

void MythPoint::CalculatePoint(const MythRect & parentArea)
{
    CalculatePoint(parentArea.toQRect());
}

void MythPoint::NormPoint(void)
{
    if (m_percentX == 0.0F)
        QPoint::setX(GetMythMainWindow()->NormX(x()));

    if (m_percentY == 0.0F)
        QPoint::setY(GetMythMainWindow()->NormY(y()));

    m_offsetX = GetMythMainWindow()->NormX(m_offsetX);
    m_offsetY = GetMythMainWindow()->NormY(m_offsetY);
}

/**
 * \brief parse the position
 *
 * \return true of position is absolute
 */
bool MythPoint::parsePosition(float & percent, int & offset, int & absolute,
                              const QString &value)
{
    /*
      Position can be either an absolute, or a percentage with an
      optional offset.

      720
      -5
      33%
      75%+10
     */

    percent = 0.0;
    offset = 0;
    absolute = 0;

    if (value.isEmpty())
        return true;

    int  number = 0;
    char ch = ' ';
    QString tmp(value); // QTextStream won't accept a const!
    QTextStream is(&tmp);

    is >> number;
    if (is.status() != QTextStream::Ok)
        return true;

    is.skipWhiteSpace();
    is >> ch;
    if (ch != '%')
    {
        absolute = number;
        return true;
    }

    percent = static_cast<float>(number) / 100.0F;
    is >> offset;
    return false;
}

void MythPoint::setX(const QString &sX)
{
    int absoluteX = 0;

    if (parsePosition(m_percentX, m_offsetX, absoluteX, sX))
        QPoint::setX(absoluteX);
    else
        m_needsUpdate = true;
}

void MythPoint::setY(const QString &sY)
{
    int absoluteY = 0;

    if (parsePosition(m_percentY, m_offsetY, absoluteY, sY))
        QPoint::setY(absoluteY);
    else
        m_needsUpdate = true;
}

QString MythPoint::getX(void) const
{
    QString stringX;
    if (m_percentX > 0.0F)
        stringX = QString("%1%").arg((int)(m_percentX * 100));
    else
        stringX = QString("%1").arg(x() - m_offsetX);
    if (m_offsetX != 0)
    {
        if (m_offsetX > 0)
            stringX += '+';
        stringX += QString("%1").arg(m_offsetX);
    }
    return stringX;
}

QString MythPoint::getY(void) const
{
    QString stringY;
    if (m_percentY > 0.0F)
        stringY = QString("%1%").arg((int)(m_percentY * 100));
    else
        stringY = QString("%1").arg(y() - m_offsetY);
    if (m_offsetY != 0)
    {
        if (m_offsetY > 0)
            stringY += '+';
        stringY += QString("%1").arg(m_offsetY);
    }
    return stringY;
}

QString MythPoint::toString(bool details) const
{
    QString result = QString("(%1,%2)")
                     .arg(x()).arg(y());

    if (details)
        result += QString(" [%1,%2]")
                      .arg(getX(), getY());

    return result;
}

QPoint MythPoint::toQPoint() const
{
    return {x(),y()};
}
