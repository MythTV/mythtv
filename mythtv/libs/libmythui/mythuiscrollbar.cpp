// Own Header
#include "mythuiscrollbar.h"

// QT
#include <QCoreApplication>
#include <QDomDocument>

// myth
#include "mythlogging.h"

const int kDefaultMaxValue = 99;
const int kDefaultPageStep = 10;

MythUIScrollBar::MythUIScrollBar(MythUIType *parent, const QString &name)
    : MythUIType(parent, name),
      m_layout(LayoutVertical), m_pageStep(kDefaultPageStep),
      m_sliderPosition(0), m_maximum(kDefaultMaxValue),
      m_hideDelay(0), m_timerId(0)
{
}

MythUIScrollBar::~MythUIScrollBar()
{
}

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
        m_hideDelay = getFirstText(element).toInt();
    else
        return MythUIType::ParseElement(filename, element, showWarnings);

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

    if (value < 1)
        value = 1;

    m_maximum = value - 1;
    CalculatePosition();
}

void MythUIScrollBar::SetSliderPosition(int value)
{
    if (value == m_sliderPosition)
        return;

    if (value < 0)
        value = 0;

    if (value > m_maximum)
        value = m_maximum;

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
        int width = qMax((int)(fillArea.width() * relativeSize + 0.5),
                         m_sliderArea.width());
        newSliderArea.setWidth(width);
        endPos.setX((int)((fillArea.width() - width) * percentage + 0.5));
    }
    else
    {
        int height = qMax((int)(fillArea.height() * relativeSize + 0.5),
                          m_sliderArea.height());
        newSliderArea.setHeight(height);
        endPos.setY((int)((fillArea.height() - height) * percentage + 0.5));
    }

    slider->SetArea(newSliderArea);
    slider->SetPosition(endPos);

    if (m_hideDelay > 0)
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
    MythUIScrollBar *scrollbar = dynamic_cast<MythUIScrollBar *>(base);
    if (!scrollbar)
        return;

    m_layout = scrollbar->m_layout;
    m_sliderArea = scrollbar->m_sliderArea;
    m_hideDelay = scrollbar->m_hideDelay;

    MythUIType::CopyFrom(base);
}

void MythUIScrollBar::CreateCopy(MythUIType *parent)
{
    MythUIScrollBar *scrollbar = new MythUIScrollBar(parent, objectName());
    scrollbar->CopyFrom(this);
}

void MythUIScrollBar::timerEvent(QTimerEvent *)
{
    if (m_timerId)
        killTimer(m_timerId);
    m_timerId = 0;
    AdjustAlpha(1, -10, 0, 255);
    connect(this, SIGNAL(FinishedFading()), this, SLOT(DoneFading()));
}

void MythUIScrollBar::DoneFading(void)
{
    disconnect(this, SIGNAL(FinishedFading()), 0, 0);
    Hide();
}
