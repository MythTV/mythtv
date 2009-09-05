
#include "mythuispinbox.h"

// QT headers
#include <QDomDocument>
#include <QCoreApplication>

MythUISpinBox::MythUISpinBox(MythUIType *parent, const QString &name)
              : MythUIButtonList(parent, name), m_hasTemplate(false)
{
}

MythUISpinBox::~MythUISpinBox()
{
}

void MythUISpinBox::SetRange(int low, int high, int step)
{
    if ((high - low) == 0 || step == 0)
        return;

    Reset();

    int value = low;

    while (value <= high)
    {
        QString text;
        if (m_hasTemplate)
        {
            if (value < 0 && !m_negativeTemplate.isEmpty())
            {
                text = qApp->translate("ThemeUI",
                                       qPrintable(m_negativeTemplate), "",
                                       QCoreApplication::CodecForTr, value);
            }
            else if (value == 0 && !m_zeroTemplate.isEmpty())
            {
                text = qApp->translate("ThemeUI",
                                       qPrintable(m_zeroTemplate), "",
                                       QCoreApplication::CodecForTr, value);
            }
            else if (!m_positiveTemplate.isEmpty())
            {
                text = qApp->translate("ThemeUI",
                                       qPrintable(m_positiveTemplate), "",
                                       QCoreApplication::CodecForTr, value);
            }
        }

        if (text.isEmpty())
            text = QString::number(value);
        
        new MythUIButtonListItem(this, text, qVariantFromValue(value));
        value = value + step;
    }

    SetPositionArrowStates();
}

bool MythUISpinBox::ParseElement(QDomElement &element)
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
        return MythUIButtonList::ParseElement(element);

    return true;
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
