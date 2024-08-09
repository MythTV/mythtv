
// Own Header
#include "mythuiprogressbar.h"

// C++
#include <algorithm>

// QT
#include <QCoreApplication>
#include <QDomDocument>

// MythBase
#include "libmythbase/mythlogging.h"

// MythUI
#include "mythuishape.h"
#include "mythuiimage.h"

void MythUIProgressBar::Reset()
{
    m_total = m_start = m_current = 0;
    CalculatePosition();
    emit DependChanged(false);
    MythUIType::Reset();
}

bool MythUIProgressBar::ParseElement(
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
    else if (element.tagName() == "style")
    {
        QString effect = getFirstText(element).toLower();

        if (effect == "slide")
            m_effect = EffectSlide;
        else
            m_effect = EffectReveal;
    }
    else
    {
        return MythUIType::ParseElement(filename, element, showWarnings);
    }

    return true;
}

void MythUIProgressBar::Set(int start, int total, int used)
{
    if (used != m_current || start != m_start || total != m_total)
    {
        m_start = start;
        m_total = total;
        SetUsed(used);
    }
}

void MythUIProgressBar::SetStart(int value)
{
    m_start = value;
    CalculatePosition();
}

void MythUIProgressBar::SetUsed(int value)
{
    m_current = std::clamp(value, m_start, m_total);
    CalculatePosition();
}

void MythUIProgressBar::SetTotal(int value)
{
    m_total = value;
    CalculatePosition();
}

void MythUIProgressBar::CalculatePosition(void)
{
    MythUIType *progressType = GetChild("progressimage");

    if (!progressType)
    {
        LOG(VB_GENERAL, LOG_ERR, "Progress image doesn't exist");
        return;
    }

    progressType->SetVisible(false);

    int total = m_total - m_start;
    int current = m_current - m_start;
    float percentage = 0.0;

    if (total <= 0 || current <= 0 || current > total)
        return;

    percentage = (float)current / (float)total;
    progressType->SetVisible(true);

    QRect fillArea = progressType->GetArea();

    int height = fillArea.height();
    int width = fillArea.width();
    int x = fillArea.x();
    int y = fillArea.y();

    switch (m_effect)
    {
        case EffectReveal :

            if (m_layout == LayoutHorizontal)
            {
                width = (int)((float)fillArea.width() * percentage);
            }
            else
            {
                height = (int)((float)fillArea.height() * percentage);
            }

            break;
        case EffectSlide :

            if (m_layout == LayoutHorizontal)
            {
                int newwidth = (int)((float)fillArea.width() * percentage);
                x = width - newwidth;
                width = newwidth;
            }
            else
            {
                int newheight = (int)((float)fillArea.height() * percentage);
                y = height - newheight;
                height = newheight;
            }

            break;
        case EffectAnimate :
            // Not implemented yet
            break;
    }

    auto *progressImage = dynamic_cast<MythUIImage *>(progressType);
    auto *progressShape = dynamic_cast<MythUIShape *>(progressType);

    if (width <= 0)
        width = 1;

    if (height <= 0)
        height = 1;

    if (progressImage)
        progressImage->SetCropRect(x, y, width, height);
    else if (progressShape)
        progressShape->SetCropRect(x, y, width, height);

    SetRedraw();
}

void MythUIProgressBar::Finalize()
{
    CalculatePosition();
}

void MythUIProgressBar::CopyFrom(MythUIType *base)
{
    auto *progressbar = dynamic_cast<MythUIProgressBar *>(base);

    if (!progressbar)
        return;

    m_layout = progressbar->m_layout;
    m_effect = progressbar->m_effect;

    m_total = progressbar->m_total;
    m_start = progressbar->m_start;
    m_current = progressbar->m_current;

    MythUIType::CopyFrom(base);
}

void MythUIProgressBar::CreateCopy(MythUIType *parent)
{
    auto *progressbar = new MythUIProgressBar(parent, objectName());
    progressbar->CopyFrom(this);
}

void MythUIProgressBar::SetVisible(bool visible)
{
    if (m_firstdepend || visible != m_visible)
    {
        emit DependChanged(!visible);
        m_firstdepend = false;
    }
    MythUIType::SetVisible(visible);
}
