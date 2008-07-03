#include <iostream>

// QT
#include <qapplication.h>

// MythDB
#include "mythverbose.h"

// MythUI
#include "mythuiprogressbar.h"

MythUIProgressBar::MythUIProgressBar(MythUIType *parent, const QString &name)
           : MythUIType(parent, name)
{
    m_layout = LayoutHorizontal;
    m_effect = EffectReveal;

    m_total = m_start = m_current = 0;
}

MythUIProgressBar::~MythUIProgressBar()
{

}

bool MythUIProgressBar::ParseElement(QDomElement &element)
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
        return MythUIType::ParseElement(element);

    return true;
}

void MythUIProgressBar::SetStart(int value)
{
    m_start = value;
    CalculatePosition();
}

void MythUIProgressBar::SetUsed(int value)
{
    m_current = value;
    CalculatePosition();
}

void MythUIProgressBar::SetTotal(int value)
{
    m_total = value;
    CalculatePosition();
}

void MythUIProgressBar::CalculatePosition(void)
{
    MythUIImage *progressImage = dynamic_cast<MythUIImage *>
                                         (GetChild("progressimage"));

    if (!progressImage)
    {
        VERBOSE(VB_IMPORTANT, "Progress image doesn't exist");
        return;
    }

    progressImage->SetVisible(false);

    int total = m_total-m_start;
    int current = m_current-m_start;
    float percentage = 0.0;

    if (total > 0 && current < total)
    {
        percentage = (float)current / (float)total;
        progressImage->SetVisible(true);
    }

    QRect fillArea = progressImage->GetArea();

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

    progressImage->SetCropRect(x,y,width,height);
    SetRedraw();
    qApp->processEvents();
}

void MythUIProgressBar::CopyFrom(MythUIType *base)
{
    MythUIProgressBar *progressbar = dynamic_cast<MythUIProgressBar *>(base);
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
    MythUIProgressBar *progressbar = new MythUIProgressBar(parent, objectName());
    progressbar->CopyFrom(this);
}
