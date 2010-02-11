
#include "mythuispinbox.h"

// QT headers
#include <QDomDocument>
#include <QCoreApplication>

MythUISpinBox::MythUISpinBox(MythUIType *parent, const QString &name)
              : MythUIButtonList(parent, name), m_hasTemplate(false),
                m_moveAmount(0)
{
}

MythUISpinBox::~MythUISpinBox()
{
}

void MythUISpinBox::SetRange(int low, int high, int step, uint pageMultiple)
{
    if ((high == low) || step == 0)
        return;

    m_moveAmount = pageMultiple;
    
    bool reverse = false;
    int value = low;
    
    if (low > high)
        reverse = true;
    
    Reset();

    while ((reverse && (value >= high)) ||
           (!reverse && (value <= high)))
    {
        QString text;
        if (m_hasTemplate)
        {
            QString temp;
            if (value < 0 && !m_negativeTemplate.isEmpty())
                temp = m_negativeTemplate;
            else if (value == 0 && !m_zeroTemplate.isEmpty())
                temp = m_zeroTemplate;
            else if (!m_positiveTemplate.isEmpty())
                temp = m_positiveTemplate;

            if (!temp.isEmpty())
                text = qApp->translate("ThemeUI", qPrintable(temp), "",
                                   QCoreApplication::CodecForTr, qAbs(value));
        }

        if (text.isEmpty())
            text = QString::number(value);
        
        new MythUIButtonListItem(this, text, qVariantFromValue(value));
        if (reverse)
            value = value - step;
        else    
            value = value + step;
    }

    SetPositionArrowStates();
}

bool MythUISpinBox::ParseElement(
    const QString &filename, QDomElement &element, bool showWarnings)    
{
    if (element.tagName() == "template")
    {
        QString format = getFirstText(element);
        if (element.attribute("type") == "negative")
            m_negativeTemplate = format;
        else if (element.attribute("type") == "zero")
            m_zeroTemplate = format;
        else
            m_positiveTemplate = format;

        m_hasTemplate = true;
    }
    else
    {
        return MythUIButtonList::ParseElement(filename, element, showWarnings);
    }

    return true;
}

bool MythUISpinBox::MoveDown(MovementUnit unit, uint amount)
{
    bool handled = false;
    if ((unit == MovePage) && m_moveAmount)
        handled = MythUIButtonList::MoveDown(MoveByAmount, m_moveAmount);
    else
        handled = MythUIButtonList::MoveDown(unit, amount);

    return handled;
}

bool MythUISpinBox::MoveUp(MovementUnit unit, uint amount)
{
    bool handled = false;
    if ((unit == MovePage) && m_moveAmount)
        handled = MythUIButtonList::MoveUp(MoveByAmount, m_moveAmount);
    else
        handled = MythUIButtonList::MoveUp(unit, amount);

    return handled;
}

void MythUISpinBox::CreateCopy(MythUIType *parent)
{
    MythUISpinBox *spinbox = new MythUISpinBox(parent, objectName());
    spinbox->CopyFrom(this);
}

void MythUISpinBox::CopyFrom(MythUIType *base)
{
    MythUISpinBox *spinbox = dynamic_cast<MythUISpinBox *>(base);
    if (!spinbox)
        return;

    m_hasTemplate = spinbox->m_hasTemplate;
    m_negativeTemplate = spinbox->m_negativeTemplate;
    m_zeroTemplate = spinbox->m_zeroTemplate;
    m_positiveTemplate = spinbox->m_positiveTemplate;
    
    MythUIButtonList::CopyFrom(base);
}
