// Own Header
#include "mythuiscrollbar.h"

// C++
#include <cmath>
#include <algorithm>

// QT
#include <QCoreApplication>
#include <QDomDocument>

// myth
#include "libmythbase/mythlogging.h"

void MythUIScrollBar::Reset()
{
    m_pageStep = kDefaultPageStep;
    m_sliderPosition = 0;
    m_maximum = kDefaultMaxValue;

    CalculatePosition();

    MythUIType::Reset();
}

bool MythUIScrollBar::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)
{
    if (element.tagName() == "layout")
    {
        QString layout = getFirstText(element).toLower();

        if (layout == "vertical")
            m_layout = LayoutVertical;
        else
            m_layout = LayoutHorizontal;
    }
    else if (element.tagName() == "hidedelay")
    {
        m_hideDelay = std::chrono::milliseconds(getFirstText(element).toInt());
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

void MythUIScrollBar::SetPageStep(int value)
{
    if (value == m_pageStep)
        return;

    if (value < 1)
        value = kDefaultPageStep;

    m_pageStep = value;
    CalculatePosition();
}

void MythUIScrollBar::SetMaximum(int value)
{
    if (value - 1 == m_maximum)
        return;

    value = std::max(value, 1);

    m_maximum = value - 1;
    CalculatePosition();
}

void MythUIScrollBar::SetSliderPosition(int value)
{
    if (value == m_sliderPosition)
        return;

    value = std::clamp(value, 0, m_maximum);

    m_sliderPosition = value;
    CalculatePosition();
}

void MythUIScrollBar::CalculatePosition(void)
{
    if (m_maximum > 0)
        Show();
    else
    {
        Hide();
        return;
    }

    MythUIType *slider = GetChild("slider");

    if (!slider)
    {
        LOG(VB_GENERAL, LOG_ERR, "Slider element doesn't exist");
        return;
    }

    float percentage = (float)m_sliderPosition / m_maximum;
    float relativeSize = (float)m_pageStep / (m_maximum + m_pageStep);

    MythRect newSliderArea = slider->GetArea();
    MythRect fillArea = GetArea();
    QPoint endPos(newSliderArea.left(), newSliderArea.top());

    if (m_layout == LayoutHorizontal)
    {
        int width = std::max((int)lroundf(fillArea.width() * relativeSize),
                         m_sliderArea.width());
        newSliderArea.setWidth(width);
        endPos.setX(lroundf((fillArea.width() - width) * percentage));
    }
    else
    {
        int height = std::max((int)lroundf(fillArea.height() * relativeSize),
                          m_sliderArea.height());
        newSliderArea.setHeight(height);
        endPos.setY(lroundf((fillArea.height() - height) * percentage));
    }

    slider->SetArea(newSliderArea);
    slider->SetPosition(endPos);

    if (m_hideDelay > 0ms)
    {
        if (m_timerId)
            killTimer(m_timerId);
        m_timerId = startTimer(m_hideDelay);

        AdjustAlpha(1, 10, 0, 255);
    }
}

void MythUIScrollBar::Finalize()
{
    MythUIType *slider = GetChild("slider");
    if (slider)
        m_sliderArea = slider->GetArea();

    CalculatePosition();
}

void MythUIScrollBar::CopyFrom(MythUIType *base)
{
    auto *scrollbar = dynamic_cast<MythUIScrollBar *>(base);
    if (!scrollbar)
        return;

    m_layout = scrollbar->m_layout;
    m_sliderArea = scrollbar->m_sliderArea;
    m_hideDelay = scrollbar->m_hideDelay;

    MythUIType::CopyFrom(base);
}

void MythUIScrollBar::CreateCopy(MythUIType *parent)
{
    auto *scrollbar = new MythUIScrollBar(parent, objectName());
    scrollbar->CopyFrom(this);
}

void MythUIScrollBar::timerEvent(QTimerEvent * /*event*/)
{
    if (m_timerId)
        killTimer(m_timerId);
    m_timerId = 0;
    AdjustAlpha(1, -10, 0, 255);
    connect(this, &MythUIType::FinishedFading, this, &MythUIScrollBar::DoneFading);
}

void MythUIScrollBar::DoneFading(void)
{
    disconnect(this, &MythUIType::FinishedFading, nullptr, nullptr);
    Hide();
}
