#include <qapplication.h>

#include "mythuiclock.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include "mythfontproperties.h"

#include "mythverbose.h"
#include "mythdb.h"

MythUIClock::MythUIClock(MythUIType *parent, const char *name)
           : MythUIText(parent, name)
{
    m_Time = QDateTime::currentDateTime();
    m_nextUpdate = m_Time.addSecs(1);
    m_Message = m_Time.toString(m_Format);

    m_DateFormat = GetMythDB()->GetSetting("DateFormat", "ddd d MMMM");
    m_ShortDateFormat = GetMythDB()->GetSetting("ShortDateFormat", "ddd d");
    m_TimeFormat = GetMythDB()->GetSetting("TimeFormat", "hh:mm");

    m_Format = QString("%1, %2").arg(m_DateFormat).arg(m_TimeFormat);

    m_SecsFlash = false;
    m_Flash = false;
}

MythUIClock::~MythUIClock()
{
    if (m_Font)
    {
        delete m_Font;
        m_Font = NULL;
    }
}

void MythUIClock::Pulse(void)
{
    m_Time = QDateTime::currentDateTime();

    if (m_Time > m_nextUpdate)
    {
        QString newMsg;

        newMsg = m_Time.toString(m_Format);
        if (m_SecsFlash)
        {
            if (m_Flash)
            {
                newMsg.replace(":", " ");
                newMsg.replace(".", " ");
                m_Flash = false;
            }
            else
                m_Flash = true;
        }

        m_CutMessage = "";

        if (m_Message != newMsg)
        {
            m_Message = newMsg;
            SetRedraw();
        }

        m_nextUpdate = m_Time.addSecs(1);
    }

    MythUIText::Pulse();
}

bool MythUIClock::ParseElement(QDomElement &element)
{
    if (element.tagName() == "format")
    {
        QString format = getFirstText(element);
        format.replace("%TIME%", m_TimeFormat, false);
        format.replace("%DATE%", m_DateFormat, false);
        format.replace("%SHORTDATE%", m_ShortDateFormat, false);
        m_Format=format;
    }
    else if (element.tagName() == "secondflash")
    {
        QString flash = getFirstText(element);
        if (flash == "yes")
        {
            m_SecsFlash = true;
        }
    }
    else
        return MythUIText::ParseElement(element);

    return true;
}

void MythUIClock::CopyFrom(MythUIType *base)
{
    MythUIClock *clock = dynamic_cast<MythUIClock *>(base);
    if (!clock)
    {
        VERBOSE(VB_IMPORTANT, "ERROR, bad parsing");
        return;
    }

    m_Time = clock->m_Time;
    m_nextUpdate = clock->m_nextUpdate;

    m_Format = clock->m_Format;
    m_TimeFormat = clock->m_TimeFormat;
    m_DateFormat = clock->m_DateFormat;
    m_ShortDateFormat = clock->m_ShortDateFormat;

    m_SecsFlash = clock->m_SecsFlash;
    m_Flash = clock->m_Flash;

    MythUIText::CopyFrom(base);
}

void MythUIClock::CreateCopy(MythUIType *parent)
{
    MythUIClock *clock = new MythUIClock(parent, objectName());
    clock->CopyFrom(this);
}

